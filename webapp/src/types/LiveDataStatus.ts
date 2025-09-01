export interface ValueObject {
    v: number; // value
    u: string; // unit
    d: number; // digits
    max: number;
}

export interface InverterStatistics {
    name: ValueObject;
    Power?: ValueObject;
    Voltage?: ValueObject;
    Current?: ValueObject;
    'Power DC'?: ValueObject;
    YieldDay?: ValueObject;
    YieldTotal?: ValueObject;
    Frequency?: ValueObject;
    Temperature?: ValueObject;
    PowerFactor?: ValueObject;
    ReactivePower?: ValueObject;
    Efficiency?: ValueObject;
    Irradiation?: ValueObject;
}

export interface RadioStatistics {
    tx_request: number;
    tx_re_request: number;
    rx_success: number;
    rx_fail_nothing: number;
    rx_fail_partial: number;
    rx_fail_corrupt: number;
    rssi: number;
}

export interface ConnectionStatisticsHoymiles {
    send_requests: number;
    received_responses: number;
    disconnects: number;
    timeouts: number;
}

export interface ConnectionStatisticsDeyeAtCommands {
    connects: number;
    connects_successful: number;
    send_commands: number;
    timout_commands: number;
    error_commands: number;
    heath_checks: number;
    heath_checks_successfully: number;
    write_requests: number;
    write_requests_successfully: number;
    read_requests: number;
    read_requests_successfully: number;
}

export interface ConnectionStatisticsCustomModbus {
    connects: number;
    connects_successful: number;
    read_requests: number;
    read_requests_successfully: number;
    write_requests: number;
    write_requests_successfully: number;
}

export interface Inverter {
    serial: string;
    name: string;
    order: number;
    manufacturer: string;
    data_age: number;
    poll_enabled: boolean;
    reachable: boolean;
    producing: boolean;
    limit_relative: number;
    limit_absolute: number;
    events: number;
    AC: InverterStatistics[];
    DC: InverterStatistics[];
    INV: InverterStatistics[];
    radio_stats: RadioStatistics;
    connection_stats_hoymiles: ConnectionStatisticsHoymiles;
    connection_stats_deye_at: ConnectionStatisticsDeyeAtCommands;
    connection_stats_deye_cust: ConnectionStatisticsCustomModbus;
}

export interface Total {
    Power: ValueObject;
    YieldDay: ValueObject;
    YieldTotal: ValueObject;
}

export interface Hints {
    time_sync: boolean;
    default_password: boolean;
    radio_problem: boolean;
    pin_mapping_issue: boolean;
}

export interface LiveData {
    inverters: Inverter[];
    total: Total;
    hints: Hints;
}
