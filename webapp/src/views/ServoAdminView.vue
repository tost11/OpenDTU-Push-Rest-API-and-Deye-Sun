<template>
  <BasePage :title="$t('servoadmin.ServoSettings')" :isLoading="dataLoading">
    <BootstrapAlert v-model="showAlert" dismissible :variant="alertType">
      {{ alertMessage }}
    </BootstrapAlert>
    <form @submit="saveServoConfig">
      <CardElement :text="$t('servoadmin.ServoConfiguration')" textVariant="text-bg-primary">
        <InputElement :label="$t('servoadmin.Enable')"
                      v-model="servoConfigList.enabled"
                      type="checkbox" wide/>

      </CardElement>

      <CardElement :text="$t('servoadmin.Parameters')" textVariant="text-bg-primary" add-space
                   v-show="servoConfigList.enabled"
      >

        <InputElement :label="$t('servoadmin.frequency')"
                      v-model="servoConfigList.frequency"
                      type="number" min="1" max="255"
                      :postfix="$t('servoadmin.unitFrequency')"
        />

        <InputElement :label="$t('servoadmin.pin')"
                      v-model="servoConfigList.pin"
                      type="number" min="1" max="255"
        />

        <InputElement :label="$t('servoadmin.rangeMin')"
                      v-model="servoConfigList.range_min"
                      type="number" min="1" max="255"
        />

        <InputElement :label="$t('servoadmin.rangeMax')"
                      v-model="servoConfigList.range_max"
                      type="number" min="1" max="255"
        />

        <InputElement :label="$t('servoadmin.serial')"
                      v-model="servoConfigList.serial"
                      type="number" min="1"
                      :tooltip="$t('servoadmin.hintSerial')"
        />

        <InputElement :label="$t('servoadmin.inputIndex')"
                      v-model="servoConfigList.input_index"
                      type="number" min="0"
                      :tooltip="$t('servoadmin.hintInputIndex')"
        />

        <InputElement :label="$t('servoadmin.power')"
                      v-model="servoConfigList.power"
                      type="number" min="1" max="65534"
                      :postfix="$t('servoadmin.unitPower')"
                      :tooltip="$t('servoadmin.hintPower')"
        />

      </CardElement>

      <button type="submit" class="btn btn-primary mb-3">{{ $t('servoadmin.Save') }}</button>
    </form>
  </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from "@/components/BootstrapAlert.vue";
import { defineComponent } from 'vue';
import {authHeader, handleResponse} from "@/utils/authentication";
import type {ServoConfig} from "@/types/ServoConfig";
import CardElement from "@/components/CardElement.vue";
import InputElement from "@/components/InputElement.vue";

export default defineComponent({
  components: {
    InputElement,
    CardElement,
    BasePage,
    BootstrapAlert
  },
  data() {
    return {
      dataLoading: true,
      alertMessage: "",
      servoConfigList: {
      } as ServoConfig,
      alertType: "info",
      showAlert: false,
    };
  },
  created() {
    this.getServoConfig();
  },
  methods: {
    getServoConfig() {
      this.dataLoading = true;
      fetch("/api/servo/config", { headers: authHeader() })
          .then((response) => handleResponse(response, this.$emitter, this.$router))
          .then((data) => {
            this.servoConfigList = data;
            this.dataLoading = false;
          });
    },
    saveServoConfig(e: Event) {
      e.preventDefault();

      const formData = new FormData();
      formData.append("data", JSON.stringify(this.servoConfigList));

      fetch("/api/servo/config", {
        method: "POST",
        headers: authHeader(),
        body: formData,
      })
          .then((response) => handleResponse(response, this.$emitter, this.$router))
          .then(
              (response) => {
                this.alertMessage = this.$t('apiresponse.' + response.code, response.param);
                this.alertType = response.type;
                this.showAlert = true;
              }
          );
    },
  },
});
</script>