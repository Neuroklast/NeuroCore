#include <JuceHeader.h>
#include "../src/utils/OpenGLErrorHandler.h"
#include "../src/utils/PerformanceMonitor.h"
#include "../src/dsl/DSLParser.h"
#include "../src/ui/DSLSyntaxHighlighter.h"

class OpenGLErrorHandlerTest : public juce::UnitTest
{
public:
    OpenGLErrorHandlerTest() : juce::UnitTest("OpenGL Error Handler") {}

    void runTest() override
    {
        beginTest("OpenGL Error Handler Basic Functions");
        
        // Test error checking enabled/disabled
        expect(OpenGLErrorHandler::isErrorCheckingEnabled());
        
        OpenGLErrorHandler::setErrorCheckingEnabled(false);
        expect(!OpenGLErrorHandler::isErrorCheckingEnabled());
        
        OpenGLErrorHandler::setErrorCheckingEnabled(true);
        expect(OpenGLErrorHandler::isErrorCheckingEnabled());
        
        // Test error string conversion
        auto errorStr = OpenGLErrorHandler::getErrorString(0); // GL_NO_ERROR
        expect(errorStr.contains("GL_NO_ERROR"));
        
        // Test clear errors (should not crash)
        OpenGLErrorHandler::clearErrors();
        
        // Test recovery attempt (should not crash)
        bool recovered = OpenGLErrorHandler::attemptErrorRecovery();
        expect(recovered); // Should succeed without OpenGL context
    }
};

class PerformanceMonitorTest : public juce::UnitTest
{
public:
    PerformanceMonitorTest() : juce::UnitTest("Performance Monitor") {}

    void runTest() override
    {
        beginTest("Performance Monitor Basic Functions");
        
        auto& monitor = PerformanceMonitor::getInstance();
        monitor.reset();
        
        // Test timing recording
        {
            PerformanceMonitor::ScopedTimer timer("testOperation");
            juce::Thread::sleep(10); // Small delay
        }
        
        double avgTime = monitor.getAverageTime("testOperation");
        expect(avgTime >= 5.0); // Should be at least 5ms due to sleep
        expect(avgTime < 50.0); // But not too high
        
        double maxTime = monitor.getMaxTime("testOperation");
        expect(maxTime >= avgTime);
        
        // Test CPU usage recording
        monitor.recordCpuUsage(45.5);
        expectWithinAbsoluteError(monitor.getCurrentCpuUsage(), 45.5, 0.1);
        
        // Test memory recording
        monitor.recordAllocation(1024, "testAlloc");
        expect(monitor.getTotalMemoryAllocated() >= 1024);
        
        // Test performance report
        auto report = monitor.getPerformanceReport();
        expect(report.contains("NeuroCore Performance Report"));
        expect(report.contains("testOperation"));
        
        // Test heavy load detection
        monitor.recordCpuUsage(85.0); // High CPU
        expect(monitor.isUnderHeavyLoad());
        
        monitor.recordCpuUsage(50.0); // Normal CPU
        expect(!monitor.isUnderHeavyLoad());
    }
};

class DSLSyntaxTest : public juce::UnitTest
{
public:
    DSLSyntaxTest() : juce::UnitTest("DSL Syntax Checking") {}

    void runTest() override
    {
        beginTest("DSL Parser Syntax Validation");
        
        dsl::DSLParser parser;
        std::vector<dsl::SyntaxError> errors;
        
        // Test valid DSL
        juce::String validDSL = R"(
param a = Gain [0 2]
param b = Frequency [20 20000]

stage1: stage y=x*a
filter1: lpf freq=b q=0.7
)";
        
        bool isValid = parser.checkSyntax(validDSL, errors);
        expect(isValid);
        expect(errors.empty());
        
        // Test invalid DSL - missing colon
        juce::String invalidDSL1 = "stage1 stage y=x*a";
        isValid = parser.checkSyntax(invalidDSL1, errors);
        expect(!isValid);
        expect(!errors.empty());
        expect(errors[0].message.contains("Missing ':'"));
        
        // Test invalid DSL - malformed parameter
        juce::String invalidDSL2 = "param = Gain [0 2]";
        errors.clear();
        isValid = parser.checkSyntax(invalidDSL2, errors);
        expect(!isValid);
        expect(!errors.empty());
        
        // Test invalid DSL - unknown block type
        juce::String invalidDSL3 = "unknown1: unknowntype arg=value";
        errors.clear();
        isValid = parser.checkSyntax(invalidDSL3, errors);
        expect(!isValid);
        expect(!errors.empty());
        expect(errors[0].message.contains("Unknown block type"));
    }
};

class DSLSyntaxHighlighterTest : public juce::UnitTest
{
public:
    DSLSyntaxHighlighterTest() : juce::UnitTest("DSL Syntax Highlighter") {}

    void runTest() override
    {
        beginTest("DSL Syntax Highlighter");
        
        DSLSyntaxHighlighter highlighter;
        
        // Test color scheme
        auto scheme = highlighter.getDefaultColourScheme();
        expect(scheme.types.size() > 0);
        
        // Test error position checking
        std::vector<dsl::SyntaxError> errors;
        dsl::SyntaxError error;
        error.line = 1;
        error.column = 5;
        error.length = 3;
        error.message = "Test error";
        error.severity = dsl::SyntaxError::Error;
        errors.push_back(error);
        
        highlighter.setSyntaxErrors(errors);
        
        expect(highlighter.hasErrorAtPosition(1, 5));
        expect(highlighter.hasErrorAtPosition(1, 6));
        expect(highlighter.hasErrorAtPosition(1, 7));
        expect(!highlighter.hasErrorAtPosition(1, 8));
        expect(!highlighter.hasErrorAtPosition(2, 5));
        
        auto errorMsg = highlighter.getErrorAtPosition(1, 5);
        expect(errorMsg == "Test error");
        
        auto noErrorMsg = highlighter.getErrorAtPosition(1, 10);
        expect(noErrorMsg.isEmpty());
    }
};

// Register the tests
static OpenGLErrorHandlerTest openGLErrorHandlerTest;
static PerformanceMonitorTest performanceMonitorTest;
static DSLSyntaxTest dslSyntaxTest;
static DSLSyntaxHighlighterTest dslSyntaxHighlighterTest;