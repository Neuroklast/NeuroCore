#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>

/**
    @class OpenGLErrorHandler
    @brief Utility class for comprehensive OpenGL error checking and handling.
    
    Provides functions to check for OpenGL errors, log them appropriately,
    and handle recovery scenarios to ensure stable operation.
*/
class OpenGLErrorHandler
{
public:
    /**
        Checks for OpenGL errors and logs them if found.
        @param operation Description of the operation being performed
        @param file Source file where the check is performed
        @param line Line number where the check is performed
        @return true if no errors were found, false if errors occurred
    */
    static bool checkOpenGLError(const char* operation, const char* file = nullptr, int line = 0);
    
    /**
        Logs an OpenGL error with context information.
        @param error The OpenGL error code
        @param operation Description of the operation that failed
        @param file Source file where the error occurred
        @param line Line number where the error occurred
    */
    static void logOpenGLError(unsigned int error, const char* operation, const char* file, int line);
    
    /**
        Attempts to recover from OpenGL errors by resetting the OpenGL state.
        @return true if recovery was successful, false otherwise
    */
    static bool attemptErrorRecovery();
    
    /**
        Clears any existing OpenGL errors without logging them.
        Useful for clearing error state before critical operations.
    */
    static void clearErrors();
    
    /**
        Converts OpenGL error code to human-readable string.
        @param error The OpenGL error code
        @return Human-readable error description
    */
    static juce::String getErrorString(unsigned int error);
    
    /**
        Sets whether OpenGL error checking is enabled.
        Can be disabled for performance-critical sections.
        @param enabled Whether to enable error checking
    */
    static void setErrorCheckingEnabled(bool enabled);
    
    /**
        @return true if error checking is currently enabled
    */
    static bool isErrorCheckingEnabled();
    
private:
    static std::atomic<bool> errorCheckingEnabled;
    static std::atomic<int> errorCount;
    static constexpr int MAX_ERRORS_PER_FRAME = 10;
    
    OpenGLErrorHandler() = delete; // Static class only
};

// Convenience macros for error checking
#define NEUROCORE_CHECK_OPENGL_ERROR(operation) \
    OpenGLErrorHandler::checkOpenGLError(operation, __FILE__, __LINE__)

#define NEUROCORE_OPENGL_CALL(call) \
    do { \
        call; \
        if (!OpenGLErrorHandler::checkOpenGLError(#call, __FILE__, __LINE__)) { \
            OpenGLErrorHandler::attemptErrorRecovery(); \
        } \
    } while(0)

// For performance critical sections where we want to disable checking temporarily
#define NEUROCORE_OPENGL_DISABLE_CHECKING() \
    bool wasEnabled = OpenGLErrorHandler::isErrorCheckingEnabled(); \
    OpenGLErrorHandler::setErrorCheckingEnabled(false)

#define NEUROCORE_OPENGL_RESTORE_CHECKING() \
    OpenGLErrorHandler::setErrorCheckingEnabled(wasEnabled)