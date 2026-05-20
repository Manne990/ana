#include "ana/ana_result.h"

const char* ana_result_name(ANA_Result result)
{
    switch (result) {
    case ANA_OK:
        return "ANA_OK";
    case ANA_ERROR_INVALID_ARGUMENT:
        return "ANA_ERROR_INVALID_ARGUMENT";
    case ANA_ERROR_UNSUPPORTED_PROFILE:
        return "ANA_ERROR_UNSUPPORTED_PROFILE";
    case ANA_ERROR_NOT_IMPLEMENTED:
        return "ANA_ERROR_NOT_IMPLEMENTED";
    default:
        return "ANA_ERROR_UNKNOWN";
    }
}

