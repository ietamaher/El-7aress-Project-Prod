#ifndef VPI_HELPERS_H
#define VPI_HELPERS_H

#include <vpi/Status.h>
#include <vpi/Types.h>
#include <stdexcept>
#include <sstream>
#include <string>
#include <iostream>  // For error logging

// Macro to check VPI status and throw runtime_error on failure
#define CHECK_VPI_STATUS(STMT)                                                                   \
    do {                                                                                         \
        VPIStatus status = (STMT);                                                               \
        if (status != VPI_SUCCESS) {                                                             \
            char buffer[VPI_MAX_STATUS_MESSAGE_LENGTH];                                          \
            vpiGetLastStatusMessage(buffer, sizeof(buffer));                                     \
            std::ostringstream ss;                                                               \
            ss << "VPI Error: " << vpiStatusGetName(status) << " (" << status << "): " << buffer \
               << " in " << __FILE__ << ":" << __LINE__ << " executing " << #STMT;               \
            std::cerr << ss.str() << std::endl; /* Log to stderr */                              \
            throw std::runtime_error(ss.str());                                                  \
        }                                                                                        \
    } while (0)

// Helper macro for safer object destruction
#define VPI_SAFE_DESTROY(FUNC, OBJ) \
    if ((OBJ) != nullptr) {         \
        FUNC(OBJ);                  \
        (OBJ) = nullptr;            \
    }

#endif  // VPI_HELPERS_H
