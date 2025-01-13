export function getInverterPortByManufacturer(manufacturer:string): number {
    if(manufacturer === "HoymilesW"){
        return 10081
    }
    if(manufacturer === "DeyeSun"){
        return 48899
    }
    return 0;
}