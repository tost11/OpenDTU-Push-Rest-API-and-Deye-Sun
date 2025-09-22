export enum LimitType {
    AbsolutNonPersistent,
    RelativNonPersistent,
    AbsolutPersistent,
    RelativPersistent,
    PowerLimitControl_Max,
}

export interface LimitConfig {
    serial: string;
    limit_value: number;
    manufacturer: String;
    limit_type: LimitType;
}
