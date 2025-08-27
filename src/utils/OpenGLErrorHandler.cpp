#include "OpenGLErrorHandler.h"

#ifdef JUCE_OPENGL
    #if JUCE_WINDOWS
        #include <windows.h>
        #include <gl/GL.h>
    #elif JUCE_MAC
        #include <OpenGL/gl.h>
    #elif JUCE_LINUX
        #include <GL/gl.h>
    #endif
#endif

std::atomic<bool> OpenGLErrorHandler::errorCheckingEnabled{true};
std::atomic<int> OpenGLErrorHandler::errorCount{0};

bool OpenGLErrorHandler::checkOpenGLError(const char* operation, const char* file, int line)
{
    if (!errorCheckingEnabled.load())
        return true;
        
#ifdef JUCE_OPENGL
    using namespace juce::gl;
    
    // Check for errors
    GLenum error = glGetError();
    if (error == GL_NO_ERROR)
        return true;
    
    // Prevent error spam
    int currentCount = errorCount.fetch_add(1);
    if (currentCount >= MAX_ERRORS_PER_FRAME)
        return false;
    
    // Log the error
    logOpenGLError(error, operation, file, line);
    
    // Check for additional errors and clear them
    while ((error = glGetError()) != GL_NO_ERROR)
    {
        if (currentCount < MAX_ERRORS_PER_FRAME)
        {
            logOpenGLError(error, operation, file, line);
            currentCount++;
        }
    }
    
    return false;
#else
    // OpenGL not available
    return true;
#endif
}

void OpenGLErrorHandler::logOpenGLError(unsigned int error, const char* operation, const char* file, int line)
{
    juce::String errorMsg = "OpenGL Error: " + getErrorString(error);
    
    if (operation)
        errorMsg += " during operation: " + juce::String(operation);
    
    if (file && line > 0)
        errorMsg += " at " + juce::String(file) + ":" + juce::String(line);
    
    // Log with different severity based on error type
#ifdef JUCE_OPENGL
    using namespace juce::gl;
    
    if (error == GL_OUT_OF_MEMORY)
    {
        // Critical error - log as error
        DBG("CRITICAL: " + errorMsg);
        juce::Logger::writeToLog("CRITICAL OPENGL ERROR: " + errorMsg);
    }
    else if (error == GL_INVALID_OPERATION || error == GL_INVALID_FRAMEBUFFER_OPERATION)
    {
        // Serious error - log as warning
        DBG("WARNING: " + errorMsg);
        juce::Logger::writeToLog("OPENGL WARNING: " + errorMsg);
    }
    else
    {
        // Less serious - debug log
        DBG(errorMsg);
    }
#endif
}

bool OpenGLErrorHandler::attemptErrorRecovery()
{
#ifdef JUCE_OPENGL
    using namespace juce::gl;
    
    try
    {
        // Clear any remaining errors
        while (glGetError() != GL_NO_ERROR) { }
        
        // Reset some basic OpenGL state that might have been corrupted
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_CULL_FACE);
        
        // Reset matrices if we're using fixed pipeline (for compatibility)
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        
        // Reset viewport to a safe state (will be corrected by component)
        glViewport(0, 0, 1, 1);
        
        // Check if recovery was successful
        return glGetError() == GL_NO_ERROR;
    }
    catch (...)
    {
        return false;
    }
#else
    return true;
#endif
}

void OpenGLErrorHandler::clearErrors()
{
#ifdef JUCE_OPENGL
    using namespace juce::gl;
    
    // Clear all pending errors
    while (glGetError() != GL_NO_ERROR) { }
    
    // Reset error count for this frame
    errorCount.store(0);
#endif
}

juce::String OpenGLErrorHandler::getErrorString(unsigned int error)
{
#ifdef JUCE_OPENGL
    using namespace juce::gl;
    
    switch (error)
    {
        case GL_NO_ERROR:
            return "GL_NO_ERROR";
        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM - An unacceptable value is specified for an enumerated argument";
        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE - A numeric argument is out of range";
        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION - The specified operation is not allowed in the current state";
        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY - There is not enough memory left to execute the command";
        case GL_STACK_OVERFLOW:
            return "GL_STACK_OVERFLOW - An attempt has been made to perform an operation that would cause an internal stack to overflow";
        case GL_STACK_UNDERFLOW:
            return "GL_STACK_UNDERFLOW - An attempt has been made to perform an operation that would cause an internal stack to underflow";
        #ifdef GL_INVALID_FRAMEBUFFER_OPERATION
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION - The framebuffer object is not complete";
        #endif
        default:
            return "Unknown OpenGL error (0x" + juce::String::toHexString((int)error) + ")";
    }
#else
    return "OpenGL not available";
#endif
}

void OpenGLErrorHandler::setErrorCheckingEnabled(bool enabled)
{
    errorCheckingEnabled.store(enabled);
    if (enabled)
        errorCount.store(0); // Reset count when re-enabling
}

bool OpenGLErrorHandler::isErrorCheckingEnabled()
{
    return errorCheckingEnabled.load();
}