export interface ServoConfig {
    enabled: boolean;
    frequency: number;
    range_min: number;
    range_max: number;
    serial: number;
    input_index: number;
    pin: number;
    power: number;
}