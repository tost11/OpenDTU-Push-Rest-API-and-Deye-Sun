// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023-2025 Thomas Basler and others
 */
#include "InverterSettings.h"
#include "Configuration.h"
#include "PinMapping.h"
#include "SunPosition.h"
#include <SpiManager.h>
#include <InverterHandler.h>

#ifdef HOYMILES
#include <Hoymiles.h>
#endif

#ifdef DEYE_SUN
#include "DeyeSun.h"
#endif

#ifdef HOYMILES_W
#include <HoymilesW.h>
#endif

#undef TAG
static const char* TAG = "invertersetup";

InverterSettingsClass InverterSettings;

InverterSettingsClass::InverterSettingsClass()
    : _settingsTask(INVERTER_UPDATE_SETTINGS_INTERVAL, TASK_FOREVER, std::bind(&InverterSettingsClass::settingsLoop, this))
    #ifdef HOYMILES
    , _hoyTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&InverterSettingsClass::hoyLoop, this))
    #endif
    #ifdef DEYE_SUN
    , _deyeTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&InverterSettingsClass::deyeLoop, this))
    #endif
    #ifdef HOYMILES_W
    , _hoyWTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&InverterSettingsClass::hoyWLoop, this))
    #endif
{
}

void InverterSettingsClass::init(Scheduler& scheduler)
{
    const CONFIG_T& config = Configuration.get();

    #ifdef DEYE_SUN
    ESP_LOGI(TAG, "Initialize Deye interface... ");
    DeyeSun.init();

    ESP_LOGI(TAG, "Setting Deye poll interval...");
    DeyeSun.setPollInterval(config.Dtu.PollInterval);
    DeyeSun.setUnknownDevicesWriteEnable(config.DeyeSettings.UnknownInverterWrite);

    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        if (config.Inverter[i].Type == inverter_type::Inverter_DeyeSun && config.Inverter[i].Serial > 0) {
            const auto& inv_cfg = config.Inverter[i];
            if (inv_cfg.Serial == 0) {
                continue;
            }

            ESP_LOGI(TAG, "Adding inverter: %0" PRIx32 "%08" PRIx32 " - %s",
                     static_cast<uint32_t>((inv_cfg.Serial >> 32) & 0xFFFFFFFF),
                     static_cast<uint32_t>(inv_cfg.Serial & 0xFFFFFFFF),
                     inv_cfg.Name);

            auto inv = DeyeSun.addInverter(
                    config.Inverter[i].Name,
                    config.Inverter[i].Serial,
                    config.Inverter[i].HostnameOrIp,
                    config.Inverter[i].Port,
                    (deye_inverter_type)config.Inverter[i].MoreInverterInfo,
                    config.Inverter[i].Username,
                    config.Inverter[i].Password);
            if (inv == nullptr) {
                ESP_LOGW(TAG, "Adding inverter failed: Unsupported type");
                continue;
            }

            inv->setPollTime(config.Inverter[i].PollTime);
            inv->setReachableThreshold(config.Inverter[i].ReachableThreshold);
            inv->setZeroValuesIfUnreachable(config.Inverter[i].ZeroRuntimeDataIfUnrechable);
            inv->setZeroYieldDayOnMidnight(config.Inverter[i].ZeroYieldDayOnMidnight);
            inv->getStatistics()->setYieldDayCorrection(config.Inverter[i].YieldDayCorrection);
            inv->getStatistics()->setDeyeSunOfflineYieldDayCorrection(config.Inverter[i].DeyeSunOfflineYieldDayCorrection);
            for (uint8_t c = 0; c < INV_MAX_CHAN_COUNT; c++) {
                inv->getStatistics()->setStringMaxPower(c, config.Inverter[i].channel[c].MaxChannelPower);
            }
            ESP_LOGI(TAG,"Adding complete");
        }
    }

    ESP_LOGI(TAG,"Initialization complete");
    #endif

    #ifdef HOYMILES
    // Initialize inverter communication
    const PinMapping_t& pin = PinMapping.get();

    // Initialize inverter communication
    ESP_LOGI(TAG, "Initialize Hoymiles interface...");
    Hoymiles.init();

    if (!PinMapping.isValidNrf24Config() && !PinMapping.isValidCmt2300Config()) {
        ESP_LOGE(TAG, "Invalid pin config");
        //return;
    }

    // Initialize NRF24 if configured
    if (PinMapping.isValidNrf24Config()) {
        ESP_LOGI(TAG, "NRF: Initialize communication");
        auto spi_bus = SpiManagerInst.claim_bus_arduino();
        ESP_ERROR_CHECK(spi_bus ? ESP_OK : ESP_FAIL);

        SPIClass* spiClass = new SPIClass(*spi_bus);
        spiClass->begin(pin.nrf24_clk, pin.nrf24_miso, pin.nrf24_mosi, pin.nrf24_cs);
        Hoymiles.initNRF(spiClass, pin.nrf24_en, pin.nrf24_irq);
    }

    // Initialize CMT2300 if configured
    if (PinMapping.isValidCmt2300Config()) {
        ESP_LOGI(TAG, "CMT2300A: Initialize communication");
        Hoymiles.initCMT(pin.cmt_sdio, pin.cmt_clk, pin.cmt_cs, pin.cmt_fcs, pin.cmt_gpio2, pin.cmt_gpio3);
        ESP_LOGI(TAG, "CMT2300A: Setting country mode...");
        Hoymiles.getRadioCmt()->setCountryMode(static_cast<CountryModeId_t>(config.Dtu.Cmt.CountryMode));
        ESP_LOGI(TAG, "CMT2300A: Setting CMT target frequency...");
        Hoymiles.getRadioCmt()->setInverterTargetFrequency(config.Dtu.Cmt.Frequency);
    }

    // Configure common radio settings
    ESP_LOGI(TAG, "RF: Setting radio PA level...");
    Hoymiles.getRadioNrf()->setPALevel((rf24_pa_dbm_e)config.Dtu.Nrf.PaLevel);
    Hoymiles.getRadioCmt()->setPALevel(config.Dtu.Cmt.PaLevel);

    ESP_LOGI(TAG, "RF: Setting DTU serial...");
    Hoymiles.getRadioNrf()->setDtuSerial(config.Dtu.Serial);
    Hoymiles.getRadioCmt()->setDtuSerial(config.Dtu.Serial);

    ESP_LOGI(TAG, "RF: Setting poll interval...");
    Hoymiles.setPollInterval(config.Dtu.PollInterval);

    // Configure inverters
    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        if (config.Inverter[i].Type == inverter_type::Inverter_Hoymiles && config.Inverter[i].Serial > 0) {
            const auto& inv_cfg = config.Inverter[i];
            if (inv_cfg.Serial == 0) {
                continue;
            }

            ESP_LOGI(TAG, "Adding inverter: %0" PRIx32 "%08" PRIx32 " - %s",
                static_cast<uint32_t>((inv_cfg.Serial >> 32) & 0xFFFFFFFF),
                static_cast<uint32_t>(inv_cfg.Serial & 0xFFFFFFFF),
                inv_cfg.Name);

            auto inv = Hoymiles.addInverter(inv_cfg.Name, inv_cfg.Serial);
            if (inv == nullptr) {
                ESP_LOGW(TAG, "Adding inverter failed: Unsupported type");
                continue;
            }

            inv->setReachableThreshold(inv_cfg.ReachableThreshold);
            inv->setZeroValuesIfUnreachable(inv_cfg.ZeroRuntimeDataIfUnrechable);
            inv->setZeroYieldDayOnMidnight(inv_cfg.ZeroYieldDayOnMidnight);
            inv->setClearEventlogOnMidnight(inv_cfg.ClearEventlogOnMidnight);
            inv->getStatistics()->setYieldDayCorrection(inv_cfg.YieldDayCorrection);
            for (uint8_t c = 0; c < INV_MAX_CHAN_COUNT; c++) {
                inv->getStatistics()->setStringMaxPower(c, inv_cfg.channel[c].MaxChannelPower);
                inv->getStatistics()->setChannelFieldOffset(TYPE_DC, static_cast<ChannelNum_t>(c), FLD_YT, inv_cfg.channel[c].YieldTotalOffset);
            }

            ESP_LOGI(TAG, "Adding complete");
        }
    }
    ESP_LOGI(TAG, "Initialization complete");
    #endif

    #ifdef HOYMILES_W
    ESP_LOGI(TAG, "Initialize HoymilesW interface...");
    //HoymilesW.setMessageOutput(&MessageOutput);
    HoymilesW.init();
    ESP_LOGI(TAG, "Setting HoymilesW poll interval...");
    HoymilesW.setPollInterval(config.Dtu.PollInterval);

    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        if (config.Inverter[i].Type == inverter_type::Inverter_HoymilesW && config.Inverter[i].Serial > 0) {
            const auto& inv_cfg = config.Inverter[i];
            if (inv_cfg.Serial == 0) {
                continue;
            }

            ESP_LOGI(TAG, "Adding inverter: %0" PRIx32 "%08" PRIx32 " - %s",
                     static_cast<uint32_t>((inv_cfg.Serial >> 32) & 0xFFFFFFFF),
                     static_cast<uint32_t>(inv_cfg.Serial & 0xFFFFFFFF),
                     inv_cfg.Name);

            auto inv = HoymilesW.addInverter(
                    config.Inverter[i].Name,
                    config.Inverter[i].Serial,
                    config.Inverter[i].HostnameOrIp,
                    config.Inverter[i].Port);
            if (inv == nullptr) {
                ESP_LOGW(TAG, "Adding inverter failed: Unsupported type");
                continue;
            }

            inv->setPollTime(config.Inverter[i].PollTime);
            inv->setReachableThreshold(config.Inverter[i].ReachableThreshold);
            inv->setZeroValuesIfUnreachable(config.Inverter[i].ZeroRuntimeDataIfUnrechable);
            inv->setZeroYieldDayOnMidnight(config.Inverter[i].ZeroYieldDayOnMidnight);
            inv->getStatistics()->setYieldDayCorrection(config.Inverter[i].YieldDayCorrection);
            inv->getStatistics()->setDeyeSunOfflineYieldDayCorrection(config.Inverter[i].DeyeSunOfflineYieldDayCorrection);
            for (uint8_t c = 0; c < INV_MAX_CHAN_COUNT; c++) {
                inv->getStatistics()->setStringMaxPower(c, config.Inverter[i].channel[c].MaxChannelPower);
            }
            ESP_LOGI(TAG, "Adding complete");
        }
    }
    ESP_LOGI(TAG, "Initialization complete");
    #endif

    #ifdef HOYMILES
    scheduler.addTask(_hoyTask);
    _hoyTask.enable();
    #endif

    #ifdef DEYE_SUN
    scheduler.addTask(_deyeTask);
    _deyeTask.enable();
    #endif

    #ifdef HOYMILES_W
    scheduler.addTask(_hoyWTask);
    _hoyWTask.enable();
    #endif

    scheduler.addTask(_settingsTask);
    _settingsTask.enable();

    ESP_LOGI(TAG, "Initialize InverterHandler...");
    InverterHandler.init();
    ESP_LOGI(TAG, "done");
}

void InverterSettingsClass::settingsLoop()
{
    const CONFIG_T& config = Configuration.get();
    const bool isDayPeriod = SunPosition.isDayPeriod();

    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        auto const& inv_cfg = config.Inverter[i];
        if (inv_cfg.Serial == 0) {
            continue;
        }
        auto inv = InverterHandler.getInverterBySerial(inv_cfg.Serial,inv_cfg.Type);
        if (inv == nullptr) {
            continue;
        }

        inv->setEnablePolling(inv_cfg.Poll_Enable && (isDayPeriod || inv_cfg.Poll_Enable_Night));
        inv->setEnableCommands(inv_cfg.Command_Enable && (isDayPeriod || inv_cfg.Command_Enable_Night));
    }
}

#ifdef HOYMILES
void InverterSettingsClass::hoyLoop()
{
    Hoymiles.loop();
}
#endif

#ifdef DEYE_SUN
void InverterSettingsClass::deyeLoop()
{
    DeyeSun.loop();
}
#endif

#ifdef HOYMILES_W
void InverterSettingsClass::hoyWLoop()
{
    HoymilesW.loop();
}
#endif
