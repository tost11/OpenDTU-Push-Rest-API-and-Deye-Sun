<template>
    <BasePage :title="$t('tostadmin.TostSettings')" :isLoading="dataLoading">
        <BootstrapAlert v-model="showAlert" dismissible :variant="alertType">
            {{ alertMessage }}
        </BootstrapAlert>
      <form @submit="saveTostConfig">
        <CardElement :text="$t('tostadmin.TostConfiguration')" textVariant="text-bg-primary">
          <InputElement :label="$t('tostadmin.EnableTost')"
                        v-model="tostConfigList.tost_enabled"
                        type="checkbox" wide/>

        </CardElement>

        <CardElement :text="$t('tostadmin.TostParamters')" textVariant="text-bg-primary" add-space
                     v-show="tostConfigList.tost_enabled"
        >
          <InputElement :label="$t('tostadmin.Url')"
                        v-model="tostConfigList.tost_url"
                        type="text" maxlength="128"
                        :placeholder="$t('tostadmin.UrlHint')"/>

          <InputElement :label="$t('tostadmin.SystemId')"
                        v-model="tostConfigList.tost_system_id"
                        type="text" maxlength="64"
                        :placeholder="$t('tostadmin.SystemIdHint')"/>

          <InputElement :label="$t('tostadmin.Token')"
                        v-model="tostConfigList.tost_token"
                        type="password" maxlength="64"
                        :placeholder="$t('tostadmin.TokenHint')"/>

          <InputElement :label="$t('tostadmin.Duration')"
                        v-model="tostConfigList.tost_duration"
                        type="number" min="1" max="1000"/>

        </CardElement>

        <button type="submit" class="btn btn-primary mb-3">{{ $t('tostadmin.Save') }}</button>
      </form>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from "@/components/BootstrapAlert.vue";
import { defineComponent } from 'vue';
import {authHeader, handleResponse} from "@/utils/authentication";
import type {TostConfig} from "@/types/TostConfig";
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
            tostConfigList: {} as TostConfig,
            alertType: "info",
            showAlert: false,
        };
    },
    created() {
      this.getTostConfig();
    },
    methods: {
      getTostConfig() {
        this.dataLoading = true;
        fetch("/api/tost/config", { headers: authHeader() })
            .then((response) => handleResponse(response, this.$emitter, this.$router))
            .then((data) => {
              this.tostConfigList = data;
              this.dataLoading = false;
            });
      },
      saveTostConfig(e: Event) {
        e.preventDefault();

        const formData = new FormData();
        formData.append("data", JSON.stringify(this.tostConfigList));

        fetch("/api/tost/config", {
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
