<template>
    <BasePage :title="$t('tostinfo.tostinformation')" :isLoading="dataLoading" :show-reload="true" @reload="gettostinfo">
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
                            <th>{{ $t('tostinfo.Server') }}</th>
                            <td>{{ tostDataList.tost_url }}</td>
                        </tr>
                        <tr>
                            <th>{{ $t('tostinfo.Username') }}</th>
                            <td>{{ tostDataList.tost_system_id }}</td>
                        </tr>
                        <tr>
                          <th>{{ $t('tostinfo.Port') }}</th>
                          <td>{{ tostDataList.tost_duration }}</td>
                        </tr>
                    </tbody>
                </table>
            </div>
        </CardElement>

        <CardElement :text="$t('tostinfo.HassSummary')" textVariant="text-bg-primary" add-space>
            <div class="table-responsive">
                <table class="table table-hover table-condensed">
                    <tbody>
                        <tr>
                            <th>{{ $t('tostinfo.LastSuccesTimestamp') }}</th>
                            <td>{{ tostDataList.tost_status_successfully_timestamp }}</td>
                        </tr>
                        <tr>
                          <th>{{ $t('tostinfo.LastErrorCode') }}</th>
                          <td>{{ tostDataList.tost_status_error_static_code }}</td>
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
        };
    },
    created() {
        this.gettostinfo();
    },
    methods: {
        gettostinfo() {
            this.dataLoading = true;
            fetch("/api/tost/status", { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.tostDataList = data;
                    this.dataLoading = false;
                });
        },
    },
});
</script>
