<template>
    <CardElement :text="$t('wifistationDevices.wifistationDevices')" textVariant="text-bg-primary">
        <div class="table-responsive">
            <table class="table table-hover table-condensed">
                <tbody>
                    <tr>
                        <th>{{ $t('IP') }}</th>
                        <td>{{ $t('Mac') }}</td>
                    </tr>
                    <template v-for="event in networkStatus.ap_station_devices" :key="event">
                        <tr>
                            <td>{{ event.ip }}</td>
                            <td>{{ event.mac }}</td>
                        </tr>
                    </template>
                </tbody>
            </table>
        </div>
    </CardElement>
</template>

<script lang="ts">
import CardElement from '@/components/CardElement.vue';
import type { NetworkStatus } from '@/types/NetworkStatus';
import { defineComponent, type PropType } from 'vue';

export default defineComponent({
    components: {
        CardElement
    },
    props: {
        networkStatus: { type: Object as PropType<NetworkStatus>, required: true },
    },
    methods: {
    },
});
</script>
