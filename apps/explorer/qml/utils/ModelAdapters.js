.pragma library

function isModelLike(value) {
    return value !== undefined && value !== null
        && typeof value.get === "function"
        && typeof value.count === "number"
}

function isListLike(value) {
    return isModelLike(value)
        || Array.isArray(value)
        || (value !== undefined && value !== null
            && typeof value !== "string"
            && typeof value.length === "number"
            && value.length >= 0)
}

function listCount(value) {
    if (value === undefined || value === null)
        return 0
    if (isModelLike(value))
        return value.count
    if (Array.isArray(value))
        return value.length
    if (isListLike(value))
        return value.length
    return 0
}

function listAt(value, index) {
    if (value === undefined || value === null || index < 0)
        return undefined
    if (isModelLike(value))
        return value.get(index)
    if (Array.isArray(value))
        return value[index]
    if (isListLike(value))
        return value[index]
    return undefined
}

function stringValue(value) {
    var scalar = value
    if (scalar !== undefined && scalar !== null && typeof scalar === "object") {
        if (scalar.value !== undefined)
            scalar = scalar.value
        else if (scalar.modelData !== undefined)
            scalar = scalar.modelData
    }
    if (scalar === undefined || scalar === null)
        return ""
    return String(scalar)
}

function stringList(value) {
    if (value === undefined || value === null)
        return []

    if (isListLike(value)) {
        var values = []
        for (var i = 0; i < listCount(value); i++) {
            var normalized = stringValue(listAt(value, i))
            if (normalized !== "")
                values.push(normalized)
        }
        return values
    }

    var scalar = stringValue(value)
    return scalar === "" ? [] : [scalar]
}

function semanticCount(value) {
    if (isListLike(value))
        return listCount(value)
    return stringValue(value) === "" ? 0 : 1
}

function semanticAt(value, index) {
    if (isListLike(value))
        return listAt(value, index)
    return index === 0 ? value : undefined
}

function stringListEquals(a, b) {
    var aCount = semanticCount(a)
    var bCount = semanticCount(b)
    var aIndex = 0
    var bIndex = 0

    while (true) {
        var aValue = ""
        while (aIndex < aCount && aValue === "")
            aValue = stringValue(semanticAt(a, aIndex++))

        var bValue = ""
        while (bIndex < bCount && bValue === "")
            bValue = stringValue(semanticAt(b, bIndex++))

        var aDone = aIndex >= aCount && aValue === ""
        var bDone = bIndex >= bCount && bValue === ""
        if (aDone || bDone)
            return aDone && bDone
        if (aValue !== bValue)
            return false
    }
}
