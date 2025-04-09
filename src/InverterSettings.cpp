// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023-2024 Thomas Basler and others
 */
#include "InverterSettings.h"
#include "Configuration.h"
#include <MessageOutput.h>
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
    MessageOutput.print("Initialize Deye interface... ");
    DeyeSun.init();

    MessageOutput.println("  Setting Deye poll interval... ");
    DeyeSun.setPollInterval(config.Dtu.PollInterval);
    DeyeSun.setUnknownDevicesWriteEnable(config.DeyeSettings.UnknownInverterWrite);

    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        if (config.Inverter[i].Type == inverter_type::Inverter_DeyeSun && config.Inverter[i].Serial > 0) {
            MessageOutput.print("  Adding DeyeSun inverter: ");
            MessageOutput.print(config.Inverter[i].Serial, HEX);
            MessageOutput.print(" - ");
            MessageOutput.print(config.Inverter[i].Name);
            auto inv = DeyeSun.addInverter(
                    config.Inverter[i].Name,
                    config.Inverter[i].Serial,
                    config.Inverter[i].HostnameOrIp,
                    config.Inverter[i].Port);

            if (inv != nullptr) {
                inv->setPollTime(config.Inverter[i].PollTime);
                inv->setReachableThreshold(config.Inverter[i].ReachableThreshold);
                inv->setZeroValuesIfUnreachable(config.Inverter[i].ZeroRuntimeDataIfUnrechable);
                inv->setZeroYieldDayOnMidnight(config.Inverter[i].ZeroYieldDayOnMidnight);
                for (uint8_t c = 0; c < INV_MAX_CHAN_COUNT; c++) {
                    inv->getStatistics()->setStringMaxPower(c, config.Inverter[i].channel[c].MaxChannelPower);
                }
            }
            MessageOutput.println(" done");
        }
    }

    MessageOutput.println("done");
    #endif

    #ifdef HOYMILES
    // Initialize inverter communication
    const PinMapping_t& pin = PinMapping.get();

    MessageOutput.print("Initialize Hoymiles interface... ");

    Hoymiles.init();

    if (PinMapping.isValidNrf24Config() || PinMapping.isValidCmt2300Config()) {
        if (PinMapping.isValidNrf24Config()) {
            auto spi_bus = SpiManagerInst.claim_bus_arduino();
            ESP_ERROR_CHECK(spi_bus ? ESP_OK : ESP_FAIL);

            SPIClass* spiClass = new SPIClass(*spi_bus);
            spiClass->begin(pin.nrf24_clk, pin.nrf24_miso, pin.nrf24_mosi, pin.nrf24_cs);
            Hoymiles.initNRF(spiClass, pin.nrf24_en, pin.nrf24_irq);
        }

        if (PinMapping.isValidCmt2300Config()) {
            Hoymiles.initCMT(pin.cmt_sdio, pin.cmt_clk, pin.cmt_cs, pin.cmt_fcs, pin.cmt_gpio2, pin.cmt_gpio3);
            MessageOutput.println("  Setting country mode... ");
            Hoymiles.getRadioCmt()->setCountryMode(static_cast<CountryModeId_t>(config.Dtu.Cmt.CountryMode));
            MessageOutput.println("  Setting CMT target frequency... ");
            Hoymiles.getRadioCmt()->setInverterTargetFrequency(config.Dtu.Cmt.Frequency);
        }

        MessageOutput.println("  Setting radio PA level... ");
        Hoymiles.getRadioNrf()->setPALevel((rf24_pa_dbm_e)config.Dtu.Nrf.PaLevel);
        Hoymiles.getRadioCmt()->setPALevel(config.Dtu.Cmt.PaLevel);

        MessageOutput.println("  Setting DTU serial... ");
        Hoymiles.getRadioNrf()->setDtuSerial(config.Dtu.Serial);
        Hoymiles.getRadioCmt()->setDtuSerial(config.Dtu.Serial);

        MessageOutput.println("  Setting poll interval... ");
        Hoymiles.setPollInterval(config.Dtu.PollInterval);

        for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
            if (config.Inverter[i].Type == inverter_type::Inverter_Hoymiles && config.Inverter[i].Serial > 0) {
                MessageOutput.printf("  Adding inverter: %0" PRIx32 "%08" PRIx32 " - %s",
                    static_cast<uint32_t>((config.Inverter[i].Serial >> 32) & 0xFFFFFFFF),
                    static_cast<uint32_t>(config.Inverter[i].Serial & 0xFFFFFFFF),
                    config.Inverter[i].Name);
                auto inv = Hoymiles.addInverter(
                    config.Inverter[i].Name,
                    config.Inverter[i].Serial);

                if (inv != nullptr) {
                    inv->setPollTime(config.Inverter[i].PollTime);
                    inv->setReachableThreshold(config.Inverter[i].ReachableThreshold);
                    inv->setZeroValuesIfUnreachable(config.Inverter[i].ZeroRuntimeDataIfUnrechable);
                    inv->setZeroYieldDayOnMidnight(config.Inverter[i].ZeroYieldDayOnMidnight);
                    inv->setClearEventlogOnMidnight(config.Inverter[i].ClearEventlogOnMidnight);
                    inv->getStatistics()->setYieldDayCorrection(config.Inverter[i].YieldDayCorrection);
                    for (uint8_t c = 0; c < INV_MAX_CHAN_COUNT; c++) {
                        inv->getStatistics()->setStringMaxPower(c, config.Inverter[i].channel[c].MaxChannelPower);
                        inv->getStatistics()->setChannelFieldOffset(TYPE_DC, static_cast<ChannelNum_t>(c), FLD_YT, config.Inverter[i].channel[c].YieldTotalOffset);
                    }
                }
                MessageOutput.println(" done");
            }
        }
        MessageOutput.println("done");
    } else {
        MessageOutput.println("Invalid pin config");
    }
    #endif

    #ifdef HOYMILES_W
    MessageOutput.print("Initialize HoymilesW interface... ");
    //HoymilesW.setMessageOutput(&MessageOutput);
    HoymilesW.init();
    MessageOutput.println("  Setting HoymilesW poll interval... ");
    HoymilesW.setPollInterval(config.Dtu.PollInterval);

    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        if (config.Inverter[i].Type == inverter_type::Inverter_HoymilesW && config.Inverter[i].Serial > 0) {
            MessageOutput.print("  Adding HoymilesW inverter: ");
            MessageOutput.print(config.Inverter[i].Serial, HEX);
            MessageOutput.print(" - ");
            MessageOutput.print(config.Inverter[i].Name);
            auto inv = HoymilesW.addInverter(
                    config.Inverter[i].Name,
                    config.Inverter[i].Serial,
                    config.Inverter[i].HostnameOrIp,
                    config.Inverter[i].Port);

            if (inv != nullptr) {
                inv->setPollTime(config.Inverter[i].PollTime);
                inv->setReachableThreshold(config.Inverter[i].ReachableThreshold);
                inv->setZeroValuesIfUnreachable(config.Inverter[i].ZeroRuntimeDataIfUnrechable);
                inv->setZeroYieldDayOnMidnight(config.Inverter[i].ZeroYieldDayOnMidnight);
                for (uint8_t c = 0; c < INV_MAX_CHAN_COUNT; c++) {
                    inv->getStatistics()->setStringMaxPower(c, config.Inverter[i].channel[c].MaxChannelPower);
                }
            }
            MessageOutput.println(" done");
        }
    }
    MessageOutput.println("done");
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

    MessageOutput.print("Initialize InverterHandler... ");
    InverterHandler.init();
    MessageOutput.println(" done");
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
