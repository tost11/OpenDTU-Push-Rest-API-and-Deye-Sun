export function getInverterPortByManufacturer(manufacturer:string,deyeProtocol: number): number {
    if(manufacturer === "HoymilesW"){
        return 10081
    }
    if(manufacturer === "DeyeSun"){
        if(deyeProtocol == 1 || deyeProtocol == 2){
            return 8899
        }
        return 48899
    }
    if(manufacturer === "HiFlowBLE"){
        return 0
    }
    return 0;
}