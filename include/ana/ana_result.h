#ifndef ANA_RESULT_H
#define ANA_RESULT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ANA_Result {
    ANA_OK = 0,
    ANA_ERROR_INVALID_ARGUMENT = -1,
    ANA_ERROR_UNSUPPORTED_PROFILE = -2,
    ANA_ERROR_NOT_IMPLEMENTED = -3
} ANA_Result;

const char* ana_result_name(ANA_Result result);

#ifdef __cplusplus
}
#endif

#endif

