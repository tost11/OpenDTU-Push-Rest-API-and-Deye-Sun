<template>
    <BasePage :title="$t('tostinfo.tostInformation')" :isLoading="dataLoading" :show-reload="true" @reload="gettostinfo">
        <CardElement :text="$t('tostinfo.ConfigurationSummary')" textVariant="text-bg-primary">
            <div class="table-responsive">
                <table class="table table-hover table-condensed">
                    <tbody>
                        <tr>
                            <th>{{ $t('tostinfo.Status') }}</th>
                            <td>
                                <StatusBadge :status="tostDataList.tost_enabled" true_text="tostinfo.Enabled" false_text="tostinfo.Disabled" />
                            </td>
                        </tr>
                        <tr>
                            <th>{{ $t('tostinfo.Url') }}</th>
                            <td>{{ tostDataList.tost_url }}</td>
                        </tr>
                        <tr>
                            <th>{{ $t('tostinfo.SystemId') }}</th>
                            <td>{{ tostDataList.tost_system_id }}</td>
                        </tr>
                        <tr>
                          <th>{{ $t('tostinfo.Duration') }}</th>
                          <td>{{ $t('tostinfo.Seconds', { sec: tostDataList.tost_duration }) }}</td>
                        </tr>
                    </tbody>
                </table>
            </div>
        </CardElement>

        <CardElement :text="$t('tostinfo.Summary')" textVariant="text-bg-primary" add-space>
            <div class="table-responsive">
                <table class="table table-hover table-condensed">
                    <tbody>
                        <tr>
                          <th>{{ $t('tostinfo.LastSuccessTimestamp') }}</th>
                          <td>{{ successDate }}</td>
                        </tr>
                    </tbody>
                </table>
            </div>
        </CardElement>
        <CardElement v-show="showError()" :text="$t('tostinfo.Error')" textVariant="text-bg-secondary" add-space>
            <div class="table-responsive">
                <table class="table table-hover table-condensed">
                    <tbody>
                        <tr>
                          <th>{{ $t('tostinfo.LastErrorTimestamp') }}</th>
                          <td>{{ errorDate }}</td>
                        </tr>
                        <tr>
                          <th>{{ $t('tostinfo.LastErrorCode') }}</th>
                          <td>{{ tostDataList.tost_status_error_code }}</td>
                        </tr>
                        <tr>
                          <th>{{ $t('tostinfo.LastErrorMessage') }}</th>
                          <td>{{ tostDataList.tost_status_error_message }}</td>
                        </tr>
                    </tbody>
                </table>
            </div>
        </CardElement>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import CardElement from '@/components/CardElement.vue';
import StatusBadge from '@/components/StatusBadge.vue';
import { authHeader, handleResponse } from '@/utils/authentication';
import { defineComponent } from 'vue';
import type {TostStatus} from "@/types/TostStatus";

export default defineComponent({
    components: {
        BasePage,
        CardElement,
        StatusBadge
    },
    data() {
        return {
            dataLoading: true,
            tostDataList: {} as TostStatus,
            errorDate: "",
            successDate: ""
        };
    },
    created() {
        this.gettostinfo();
    },
    methods: {
        secondsToDhms(seconds){
          seconds = Number(seconds);
          if(seconds == 0){
            return this.$t('tostinfo.never')
          }
          var d = Math.floor(seconds / (3600*24));
          var h = Math.floor(seconds % (3600*24) / 3600);
          var m = Math.floor(seconds % 3600 / 60);
          var s = Math.floor(seconds % 60);

          var dDisplay = d > 0 ? d + (d == 1 ? " day, " : " days, ") : "";
          var hDisplay = h > 0 ? h + (h == 1 ? " hour, " : " hours, ") : "";
          var mDisplay = m > 0 ? m + (m == 1 ? " minute, " : " minutes, ") : "";
          var sDisplay = s > 0 ? s + (s == 1 ? " second" : " seconds") : "";
          return dDisplay + hDisplay + mDisplay + sDisplay;
        },
        gettostinfo() {
            this.dataLoading = true;
            fetch("/api/tost/status", { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.tostDataList = data;
                    this.dataLoading = false;
                    this.successDate = this.secondsToDhms(this.tostDataList.tost_status_successfully_timestamp/1000)
                    this.errorDate = this.secondsToDhms(this.tostDataList.tost_status_error_timestamp/1000)
                });
        },
        showError(){
          return this.tostDataList.tost_status_error_code !== 0;
        }
    },
});
</script>
