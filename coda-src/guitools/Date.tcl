proc nowString { } {
    return [exec date +%Y%b%e_%H:%M:%S]
}

proc nowSeconds { } {
    return [exec date +%s]
}

