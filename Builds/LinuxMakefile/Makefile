# Automatically generated makefile, created by the Projucer
# Don't edit this file! Your changes will be overwritten when you re-save the Projucer project!

# build with "V=1" for verbose builds
ifeq ($(V), 1)
V_AT =
else
V_AT = @
endif

# (this disables dependency generation if multiple architectures are set)
DEPFLAGS := $(if $(word 2, $(TARGET_ARCH)), , -MMD)

ifndef PKG_CONFIG
  PKG_CONFIG=pkg-config
endif

ifndef STRIP
  STRIP=strip
endif

ifndef AR
  AR=ar
endif

ifndef CONFIG
  CONFIG=Debug
endif

JUCE_ARCH_LABEL := $(shell uname -m)

ifeq ($(CONFIG),Debug)
  JUCE_BINDIR := build
  JUCE_LIBDIR := build
  JUCE_OBJDIR := build/intermediate/Debug
  JUCE_OUTDIR := build

  ifeq ($(TARGET_ARCH),)
    TARGET_ARCH := 
  endif

  JUCE_CPPFLAGS := $(DEPFLAGS) "-DLINUX=1" "-DDEBUG=1" "-D_DEBUG=1" "-DJUCER_LINUX_MAKE_6D53C8B4=1" "-DJUCE_APP_VERSION=0.0.1" "-DJUCE_APP_VERSION_HEX=0x1" "-DJUCE_USE_EXTERNAL_TEMPORARY_SUBPROCESS=1" $(shell $(PKG_CONFIG) --cflags $(shell ($(PKG_CONFIG) --exists webkit2gtk-4.1 && echo webkit2gtk-4.1) || echo webkit2gtk-4.0) alsa freetype2 fontconfig gl libcurl gtk+-x11-3.0) -pthread -I$(HOME)/JUCE/modules/juce_audio_processors/format_types/VST3_SDK -I../../JuceLibraryCode -Ipre_build -I$(HOME)/JUCE/modules -I../../modules $(CPPFLAGS)

  JUCE_CPPFLAGS_VST3 :=  "-DJucePlugin_Build_VST=0" "-DJucePlugin_Build_VST3=1" "-DJucePlugin_Build_AU=0" "-DJucePlugin_Build_AUv3=0" "-DJucePlugin_Build_AAX=0" "-DJucePlugin_Build_Standalone=0" "-DJucePlugin_Build_Unity=0" "-DJucePlugin_Build_LV2=0"
  JUCE_CFLAGS_VST3 := -fPIC -fvisibility=hidden
  JUCE_LDFLAGS_VST3 := -shared -Wl,--no-undefined
  JUCE_VST3DIR := NeuroCore.vst3
  VST3_PLATFORM_ARCH := $(shell $(CXX) make_helpers/arch_detection.cpp 2>&1 | tr '\n' ' ' | sed "s/.*JUCE_ARCH \([a-zA-Z0-9_-]*\).*/\1/")
  JUCE_VST3SUBDIR := Contents/$(VST3_PLATFORM_ARCH)-linux
  JUCE_TARGET_VST3 := $(JUCE_VST3DIR)/$(JUCE_VST3SUBDIR)/NeuroCore.so
  JUCE_VST3DESTDIR := $(HOME)/.vst3
  JUCE_COPYCMD_VST3 := $(JUCE_OUTDIR)/$(JUCE_VST3DIR) $(JUCE_VST3DESTDIR)

  JUCE_CPPFLAGS_STANDALONE_PLUGIN :=  "-DJucePlugin_Build_VST=0" "-DJucePlugin_Build_VST3=0" "-DJucePlugin_Build_AU=0" "-DJucePlugin_Build_AUv3=0" "-DJucePlugin_Build_AAX=0" "-DJucePlugin_Build_Standalone=1" "-DJucePlugin_Build_Unity=0" "-DJucePlugin_Build_LV2=0"
  JUCE_TARGET_STANDALONE_PLUGIN := NeuroCore

  JUCE_CPPFLAGS_SHARED_CODE :=  "-DJucePlugin_Build_VST=0" "-DJucePlugin_Build_VST3=1" "-DJucePlugin_Build_AU=0" "-DJucePlugin_Build_AUv3=0" "-DJucePlugin_Build_AAX=0" "-DJucePlugin_Build_Standalone=1" "-DJucePlugin_Build_Unity=0" "-DJucePlugin_Build_LV2=0" "-DJUCE_SHARED_CODE=1"
  JUCE_CFLAGS_SHARED_CODE := -fPIC -fvisibility=hidden
  JUCE_TARGET_SHARED_CODE := NeuroCore.a

  JUCE_CPPFLAGS_VST3_MANIFEST_HELPER :=  "-DJucePlugin_Build_VST=0" "-DJucePlugin_Build_VST3=0" "-DJucePlugin_Build_AU=0" "-DJucePlugin_Build_AUv3=0" "-DJucePlugin_Build_AAX=0" "-DJucePlugin_Build_Standalone=0" "-DJucePlugin_Build_Unity=0" "-DJucePlugin_Build_LV2=0"
  JUCE_TARGET_VST3_MANIFEST_HELPER := juce_vst3_helper

  JUCE_CFLAGS += $(JUCE_CPPFLAGS) $(TARGET_ARCH) -fPIC -g -ggdb -O0 $(CFLAGS)
  JUCE_CXXFLAGS += $(JUCE_CFLAGS) -std=c++20 $(CXXFLAGS)
  JUCE_LDFLAGS += $(TARGET_ARCH) -L$(JUCE_BINDIR) -L$(JUCE_LIBDIR) $(shell $(PKG_CONFIG) --libs alsa freetype2 fontconfig gl libcurl) -fvisibility=hidden -lrt -ldl -lpthread $(LDFLAGS)

  CLEANCMD = rm -rf $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3) $(JUCE_OUTDIR)/$(JUCE_TARGET_STANDALONE_PLUGIN) $(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE) $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3_MANIFEST_HELPER) $(JUCE_OBJDIR) pre_build
endif

ifeq ($(CONFIG),Release)
  JUCE_BINDIR := build
  JUCE_LIBDIR := build
  JUCE_OBJDIR := build/intermediate/Release
  JUCE_OUTDIR := build

  ifeq ($(TARGET_ARCH),)
    TARGET_ARCH := 
  endif

  JUCE_CPPFLAGS := $(DEPFLAGS) "-DLINUX=1" "-DNDEBUG=1" "-DJUCER_LINUX_MAKE_6D53C8B4=1" "-DJUCE_APP_VERSION=0.0.1" "-DJUCE_APP_VERSION_HEX=0x1" "-DJUCE_USE_EXTERNAL_TEMPORARY_SUBPROCESS=1" $(shell $(PKG_CONFIG) --cflags $(shell ($(PKG_CONFIG) --exists webkit2gtk-4.1 && echo webkit2gtk-4.1) || echo webkit2gtk-4.0) alsa freetype2 fontconfig gl libcurl gtk+-x11-3.0) -pthread -I$(HOME)/JUCE/modules/juce_audio_processors/format_types/VST3_SDK -I../../JuceLibraryCode -Ipre_build -I$(HOME)/JUCE/modules -I../../modules $(CPPFLAGS)

  JUCE_CPPFLAGS_VST3 :=  "-DJucePlugin_Build_VST=0" "-DJucePlugin_Build_VST3=1" "-DJucePlugin_Build_AU=0" "-DJucePlugin_Build_AUv3=0" "-DJucePlugin_Build_AAX=0" "-DJucePlugin_Build_Standalone=0" "-DJucePlugin_Build_Unity=0" "-DJucePlugin_Build_LV2=0"
  JUCE_CFLAGS_VST3 := -fPIC -fvisibility=hidden
  JUCE_LDFLAGS_VST3 := -shared -Wl,--no-undefined
  JUCE_VST3DIR := NeuroCore.vst3
  VST3_PLATFORM_ARCH := $(shell $(CXX) make_helpers/arch_detection.cpp 2>&1 | tr '\n' ' ' | sed "s/.*JUCE_ARCH \([a-zA-Z0-9_-]*\).*/\1/")
  JUCE_VST3SUBDIR := Contents/$(VST3_PLATFORM_ARCH)-linux
  JUCE_TARGET_VST3 := $(JUCE_VST3DIR)/$(JUCE_VST3SUBDIR)/NeuroCore.so
  JUCE_VST3DESTDIR := $(HOME)/.vst3
  JUCE_COPYCMD_VST3 := $(JUCE_OUTDIR)/$(JUCE_VST3DIR) $(JUCE_VST3DESTDIR)

  JUCE_CPPFLAGS_STANDALONE_PLUGIN :=  "-DJucePlugin_Build_VST=0" "-DJucePlugin_Build_VST3=0" "-DJucePlugin_Build_AU=0" "-DJucePlugin_Build_AUv3=0" "-DJucePlugin_Build_AAX=0" "-DJucePlugin_Build_Standalone=1" "-DJucePlugin_Build_Unity=0" "-DJucePlugin_Build_LV2=0"
  JUCE_TARGET_STANDALONE_PLUGIN := NeuroCore

  JUCE_CPPFLAGS_SHARED_CODE :=  "-DJucePlugin_Build_VST=0" "-DJucePlugin_Build_VST3=1" "-DJucePlugin_Build_AU=0" "-DJucePlugin_Build_AUv3=0" "-DJucePlugin_Build_AAX=0" "-DJucePlugin_Build_Standalone=1" "-DJucePlugin_Build_Unity=0" "-DJucePlugin_Build_LV2=0" "-DJUCE_SHARED_CODE=1"
  JUCE_CFLAGS_SHARED_CODE := -fPIC -fvisibility=hidden
  JUCE_TARGET_SHARED_CODE := NeuroCore.a

  JUCE_CPPFLAGS_VST3_MANIFEST_HELPER :=  "-DJucePlugin_Build_VST=0" "-DJucePlugin_Build_VST3=0" "-DJucePlugin_Build_AU=0" "-DJucePlugin_Build_AUv3=0" "-DJucePlugin_Build_AAX=0" "-DJucePlugin_Build_Standalone=0" "-DJucePlugin_Build_Unity=0" "-DJucePlugin_Build_LV2=0"
  JUCE_TARGET_VST3_MANIFEST_HELPER := juce_vst3_helper

  JUCE_CFLAGS += $(JUCE_CPPFLAGS) $(TARGET_ARCH) -fPIC -O3 $(CFLAGS)
  JUCE_CXXFLAGS += $(JUCE_CFLAGS) -std=c++20 $(CXXFLAGS)
  JUCE_LDFLAGS += $(TARGET_ARCH) -L$(JUCE_BINDIR) -L$(JUCE_LIBDIR) $(shell $(PKG_CONFIG) --libs alsa freetype2 fontconfig gl libcurl) -fvisibility=hidden -lrt -ldl -lpthread $(LDFLAGS)

  CLEANCMD = rm -rf $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3) $(JUCE_OUTDIR)/$(JUCE_TARGET_STANDALONE_PLUGIN) $(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE) $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3_MANIFEST_HELPER) $(JUCE_OBJDIR) pre_build
endif

OBJECTS_ALL := \

OBJECTS_VST3 := \
  $(JUCE_OBJDIR)/include_juce_audio_plugin_client_VST3_dd633589.o \

OBJECTS_STANDALONE_PLUGIN := \
  $(JUCE_OBJDIR)/include_juce_audio_plugin_client_Standalone_1a871192.o \

OBJECTS_SHARED_CODE := \
  $(JUCE_OBJDIR)/PluginProcessor_2edbcaaf.o \
  $(JUCE_OBJDIR)/DSLParser_266e9ef4.o \
  $(JUCE_OBJDIR)/SignalChain_93c73c71.o \
  $(JUCE_OBJDIR)/AdvancedOscillatorWrapper_50422f0d.o \
  $(JUCE_OBJDIR)/BandPassFilter_50e9440c.o \
  $(JUCE_OBJDIR)/HighPassFilter_ae1418b9.o \
  $(JUCE_OBJDIR)/InputGain_219ae3a5.o \
  $(JUCE_OBJDIR)/InputRouter_3bd9c22f.o \
  $(JUCE_OBJDIR)/LookupTables_3e55ff4d.o \
  $(JUCE_OBJDIR)/LowPassFilter_54cabd19.o \
  $(JUCE_OBJDIR)/SignalPolisher_b940cf54.o \
  $(JUCE_OBJDIR)/WaveShaper_350aa8d8.o \
  $(JUCE_OBJDIR)/ParameterComponent_9a90fab7.o \
  $(JUCE_OBJDIR)/DslTerminalEditor_34e40beb.o \
  $(JUCE_OBJDIR)/FormulaDisplayComponent_81edfc26.o \
  $(JUCE_OBJDIR)/FormulaWaveComponent_751019e3.o \
  $(JUCE_OBJDIR)/InlineAutocompleteEditor_a9013d93.o \
  $(JUCE_OBJDIR)/LoudnessMeterComponent_6167a4a2.o \
  $(JUCE_OBJDIR)/PluginEditor_74bf5d25.o \
  $(JUCE_OBJDIR)/PresetOverlay_fefc70b6.o \
  $(JUCE_OBJDIR)/PresetTableComponent_1fef5133.o \
  $(JUCE_OBJDIR)/ValidationOverlay_cb0cdedc.o \
  $(JUCE_OBJDIR)/WaveformDisplayComponent_20e7125d.o \
  $(JUCE_OBJDIR)/WeightedLayout_c2f3e946.o \
  $(JUCE_OBJDIR)/ExpressionEvaluator_39236fa3.o \
  $(JUCE_OBJDIR)/FormulaHelper_731856a0.o \
  $(JUCE_OBJDIR)/Localiser_408913ae.o \
  $(JUCE_OBJDIR)/PresetManager_6bccacba.o \
  $(JUCE_OBJDIR)/main_841a31bc.o \
  $(JUCE_OBJDIR)/BinaryData_ce4232d4.o \
  $(JUCE_OBJDIR)/include_juce_animation_c2224b92.o \
  $(JUCE_OBJDIR)/include_juce_audio_basics_8a4e984a.o \
  $(JUCE_OBJDIR)/include_juce_audio_devices_63111d02.o \
  $(JUCE_OBJDIR)/include_juce_audio_formats_15f82001.o \
  $(JUCE_OBJDIR)/include_juce_audio_plugin_client_ARA_31a052ed.o \
  $(JUCE_OBJDIR)/include_juce_audio_processors_10c03666.o \
  $(JUCE_OBJDIR)/include_juce_audio_processors_ara_2a4c6ef7.o \
  $(JUCE_OBJDIR)/include_juce_audio_processors_lv2_libs_12bdca08.o \
  $(JUCE_OBJDIR)/include_juce_audio_utils_9f9fb2d6.o \
  $(JUCE_OBJDIR)/include_juce_core_f26d17db.o \
  $(JUCE_OBJDIR)/include_juce_core_CompilationTime_9257742c.o \
  $(JUCE_OBJDIR)/include_juce_cryptography_8cb807a8.o \
  $(JUCE_OBJDIR)/include_juce_data_structures_7471b1e3.o \
  $(JUCE_OBJDIR)/include_juce_dsp_aeb2060f.o \
  $(JUCE_OBJDIR)/include_juce_events_fd7d695.o \
  $(JUCE_OBJDIR)/include_juce_graphics_f817e147.o \
  $(JUCE_OBJDIR)/include_juce_graphics_Harfbuzz_60c52ba2.o \
  $(JUCE_OBJDIR)/include_juce_graphics_Sheenbidi_c310974d.o \
  $(JUCE_OBJDIR)/include_juce_gui_basics_e3f79785.o \
  $(JUCE_OBJDIR)/include_juce_gui_extra_6dee1c1a.o \
  $(JUCE_OBJDIR)/include_juce_opengl_a8a032b.o \
  $(JUCE_OBJDIR)/include_juce_osc_f3df604d.o \
  $(JUCE_OBJDIR)/include_juce_product_unlocking_8278fcdc.o \
  $(JUCE_OBJDIR)/include_melatonin_inspector_8bd0336d.o \
  $(JUCE_OBJDIR)/juce_LinuxSubprocessHelperBinaryData_e1db7dc.o \

OBJECTS_VST3_MANIFEST_HELPER := \
  $(JUCE_OBJDIR)/juce_VST3ManifestHelper_d62bf7a7.o \

.PHONY: clean all strip VST3 Standalone VST3_MANIFEST_HELPER

all : VST3 Standalone VST3_MANIFEST_HELPER

VST3 : $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3)
Standalone : $(JUCE_OUTDIR)/$(JUCE_TARGET_STANDALONE_PLUGIN)
VST3_MANIFEST_HELPER : $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3_MANIFEST_HELPER)


$(JUCE_OUTDIR)/$(JUCE_TARGET_VST3) : $(OBJECTS_VST3) $(JUCE_OBJDIR)/execinfo.cmd $(RESOURCES) $(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE) $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3_MANIFEST_HELPER)
	@command -v $(PKG_CONFIG) >/dev/null 2>&1 || { echo >&2 "pkg-config not installed. Please, install it."; exit 1; }
	@$(PKG_CONFIG) --print-errors alsa freetype2 fontconfig gl libcurl
	@echo Linking "NeuroCore - VST3"
	-$(V_AT)mkdir -p $(JUCE_BINDIR)
	-$(V_AT)mkdir -p $(JUCE_LIBDIR)
	-$(V_AT)mkdir -p $(JUCE_OUTDIR)
	-$(V_AT)mkdir -p $(JUCE_OUTDIR)/$(JUCE_VST3DIR)/$(JUCE_VST3SUBDIR)
	$(V_AT)$(CXX) -o $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3) $(OBJECTS_VST3) $(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE) $(JUCE_LDFLAGS) $(shell cat $(JUCE_OBJDIR)/execinfo.cmd) $(JUCE_LDFLAGS_VST3) $(RESOURCES) $(TARGET_ARCH)
	-$(V_AT)mkdir -p $(JUCE_OUTDIR)/$(JUCE_VST3DIR)/Contents/Resources
	-$(V_AT)rm -f $(JUCE_OUTDIR)/$(JUCE_VST3DIR)/Contents/moduleinfo.json
	$(V_AT) $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3_MANIFEST_HELPER) -create -version "0.0.1" -path $(JUCE_OUTDIR)/$(JUCE_VST3DIR) -output $(JUCE_OUTDIR)/$(JUCE_VST3DIR)/Contents/Resources/moduleinfo.json
	-$(V_AT)[ ! "$(JUCE_VST3DESTDIR)" ] || (mkdir -p $(JUCE_VST3DESTDIR) && cp -R $(JUCE_COPYCMD_VST3))

$(JUCE_OUTDIR)/$(JUCE_TARGET_STANDALONE_PLUGIN) : $(OBJECTS_STANDALONE_PLUGIN) $(JUCE_OBJDIR)/execinfo.cmd $(RESOURCES) $(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE)
	@command -v $(PKG_CONFIG) >/dev/null 2>&1 || { echo >&2 "pkg-config not installed. Please, install it."; exit 1; }
	@$(PKG_CONFIG) --print-errors alsa freetype2 fontconfig gl libcurl
	@echo Linking "NeuroCore - Standalone Plugin"
	-$(V_AT)mkdir -p $(JUCE_BINDIR)
	-$(V_AT)mkdir -p $(JUCE_LIBDIR)
	-$(V_AT)mkdir -p $(JUCE_OUTDIR)
	$(V_AT)$(CXX) -o $(JUCE_OUTDIR)/$(JUCE_TARGET_STANDALONE_PLUGIN) $(OBJECTS_STANDALONE_PLUGIN) $(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE) $(JUCE_LDFLAGS) $(shell cat $(JUCE_OBJDIR)/execinfo.cmd) $(JUCE_LDFLAGS_STANDALONE_PLUGIN) $(RESOURCES) $(TARGET_ARCH)

$(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE) : $(OBJECTS_SHARED_CODE) $(JUCE_OBJDIR)/execinfo.cmd $(RESOURCES)
	@command -v $(PKG_CONFIG) >/dev/null 2>&1 || { echo >&2 "pkg-config not installed. Please, install it."; exit 1; }
	@$(PKG_CONFIG) --print-errors alsa freetype2 fontconfig gl libcurl
	@echo Linking "NeuroCore - Shared Code"
	-$(V_AT)mkdir -p $(JUCE_BINDIR)
	-$(V_AT)mkdir -p $(JUCE_LIBDIR)
	-$(V_AT)mkdir -p $(JUCE_OUTDIR)
	$(V_AT)$(AR) -rcs $(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE) $(OBJECTS_SHARED_CODE)

$(JUCE_OUTDIR)/$(JUCE_TARGET_VST3_MANIFEST_HELPER) : $(OBJECTS_VST3_MANIFEST_HELPER) $(JUCE_OBJDIR)/execinfo.cmd $(RESOURCES) $(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE) $(JUCE_OBJDIR)/cxxfs.cmd
	@command -v $(PKG_CONFIG) >/dev/null 2>&1 || { echo >&2 "pkg-config not installed. Please, install it."; exit 1; }
	@$(PKG_CONFIG) --print-errors alsa freetype2 fontconfig gl libcurl
	@echo Linking "NeuroCore - VST3 Manifest Helper"
	-$(V_AT)mkdir -p $(JUCE_BINDIR)
	-$(V_AT)mkdir -p $(JUCE_LIBDIR)
	-$(V_AT)mkdir -p $(JUCE_OUTDIR)
	$(V_AT)$(CXX) -o $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3_MANIFEST_HELPER) $(OBJECTS_VST3_MANIFEST_HELPER) $(JUCE_OUTDIR)/$(JUCE_TARGET_SHARED_CODE) $(JUCE_LDFLAGS) $(shell cat $(JUCE_OBJDIR)/execinfo.cmd) $(shell cat $(JUCE_OBJDIR)/cxxfs.cmd) $(RESOURCES) $(TARGET_ARCH)

$(JUCE_OBJDIR)/include_juce_audio_plugin_client_VST3_dd633589.o: ../../JuceLibraryCode/include_juce_audio_plugin_client_VST3.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_plugin_client_VST3.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_VST3) $(JUCE_CFLAGS_VST3) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_plugin_client_Standalone_1a871192.o: ../../JuceLibraryCode/include_juce_audio_plugin_client_Standalone.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_plugin_client_Standalone.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_STANDALONE_PLUGIN) $(JUCE_CFLAGS_STANDALONE_PLUGIN) -o "$@" -c "$<"

$(JUCE_OBJDIR)/PluginProcessor_2edbcaaf.o: ../../src/core/PluginProcessor.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling PluginProcessor.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/DSLParser_266e9ef4.o: ../../src/dsl/DSLParser.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling DSLParser.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/SignalChain_93c73c71.o: ../../src/dsl/SignalChain.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling SignalChain.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AdvancedOscillatorWrapper_50422f0d.o: ../../src/dsp/AdvancedOscillatorWrapper.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AdvancedOscillatorWrapper.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/BandPassFilter_50e9440c.o: ../../src/dsp/BandPassFilter.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling BandPassFilter.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/HighPassFilter_ae1418b9.o: ../../src/dsp/HighPassFilter.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling HighPassFilter.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/InputGain_219ae3a5.o: ../../src/dsp/InputGain.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling InputGain.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/InputRouter_3bd9c22f.o: ../../src/dsp/InputRouter.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling InputRouter.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/LookupTables_3e55ff4d.o: ../../src/dsp/LookupTables.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling LookupTables.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/LowPassFilter_54cabd19.o: ../../src/dsp/LowPassFilter.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling LowPassFilter.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/SignalPolisher_b940cf54.o: ../../src/dsp/SignalPolisher.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling SignalPolisher.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/WaveShaper_350aa8d8.o: ../../src/dsp/WaveShaper.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling WaveShaper.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ParameterComponent_9a90fab7.o: ../../src/ui/custom/ParameterComponent.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ParameterComponent.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/DslTerminalEditor_34e40beb.o: ../../src/ui/DslTerminalEditor.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling DslTerminalEditor.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/FormulaDisplayComponent_81edfc26.o: ../../src/ui/FormulaDisplayComponent.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling FormulaDisplayComponent.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/FormulaWaveComponent_751019e3.o: ../../src/ui/FormulaWaveComponent.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling FormulaWaveComponent.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/InlineAutocompleteEditor_a9013d93.o: ../../src/ui/InlineAutocompleteEditor.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling InlineAutocompleteEditor.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/LoudnessMeterComponent_6167a4a2.o: ../../src/ui/LoudnessMeterComponent.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling LoudnessMeterComponent.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/PluginEditor_74bf5d25.o: ../../src/ui/PluginEditor.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling PluginEditor.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/PresetOverlay_fefc70b6.o: ../../src/ui/PresetOverlay.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling PresetOverlay.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/PresetTableComponent_1fef5133.o: ../../src/ui/PresetTableComponent.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling PresetTableComponent.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ValidationOverlay_cb0cdedc.o: ../../src/ui/ValidationOverlay.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ValidationOverlay.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/WaveformDisplayComponent_20e7125d.o: ../../src/ui/WaveformDisplayComponent.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling WaveformDisplayComponent.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/WeightedLayout_c2f3e946.o: ../../src/ui/WeightedLayout.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling WeightedLayout.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ExpressionEvaluator_39236fa3.o: ../../src/utils/ExpressionEvaluator.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ExpressionEvaluator.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/FormulaHelper_731856a0.o: ../../src/utils/FormulaHelper.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling FormulaHelper.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/Localiser_408913ae.o: ../../src/utils/Localiser.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling Localiser.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/PresetManager_6bccacba.o: ../../src/utils/PresetManager.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling PresetManager.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/main_841a31bc.o: ../../tests/main.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling main.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/BinaryData_ce4232d4.o: ../../JuceLibraryCode/BinaryData.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling BinaryData.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_animation_c2224b92.o: ../../JuceLibraryCode/include_juce_animation.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_animation.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_basics_8a4e984a.o: ../../JuceLibraryCode/include_juce_audio_basics.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_basics.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_devices_63111d02.o: ../../JuceLibraryCode/include_juce_audio_devices.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_devices.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_formats_15f82001.o: ../../JuceLibraryCode/include_juce_audio_formats.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_formats.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_plugin_client_ARA_31a052ed.o: ../../JuceLibraryCode/include_juce_audio_plugin_client_ARA.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_plugin_client_ARA.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_processors_10c03666.o: ../../JuceLibraryCode/include_juce_audio_processors.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_processors.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_processors_ara_2a4c6ef7.o: ../../JuceLibraryCode/include_juce_audio_processors_ara.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_processors_ara.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_processors_lv2_libs_12bdca08.o: ../../JuceLibraryCode/include_juce_audio_processors_lv2_libs.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_processors_lv2_libs.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_utils_9f9fb2d6.o: ../../JuceLibraryCode/include_juce_audio_utils.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_utils.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_core_f26d17db.o: ../../JuceLibraryCode/include_juce_core.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_core.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_core_CompilationTime_9257742c.o: ../../JuceLibraryCode/include_juce_core_CompilationTime.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_core_CompilationTime.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_cryptography_8cb807a8.o: ../../JuceLibraryCode/include_juce_cryptography.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_cryptography.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_data_structures_7471b1e3.o: ../../JuceLibraryCode/include_juce_data_structures.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_data_structures.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_dsp_aeb2060f.o: ../../JuceLibraryCode/include_juce_dsp.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_dsp.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_events_fd7d695.o: ../../JuceLibraryCode/include_juce_events.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_events.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_graphics_f817e147.o: ../../JuceLibraryCode/include_juce_graphics.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_graphics.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_graphics_Harfbuzz_60c52ba2.o: ../../JuceLibraryCode/include_juce_graphics_Harfbuzz.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_graphics_Harfbuzz.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_graphics_Sheenbidi_c310974d.o: ../../JuceLibraryCode/include_juce_graphics_Sheenbidi.c
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_graphics_Sheenbidi.c"
	$(V_AT)$(CC) $(JUCE_CFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_gui_basics_e3f79785.o: ../../JuceLibraryCode/include_juce_gui_basics.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_gui_basics.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_gui_extra_6dee1c1a.o: ../../JuceLibraryCode/include_juce_gui_extra.cpp pre_build/juce_LinuxSubprocessHelperBinaryData.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_gui_extra.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_opengl_a8a032b.o: ../../JuceLibraryCode/include_juce_opengl.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_opengl.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_osc_f3df604d.o: ../../JuceLibraryCode/include_juce_osc.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_osc.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_product_unlocking_8278fcdc.o: ../../JuceLibraryCode/include_juce_product_unlocking.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_product_unlocking.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_melatonin_inspector_8bd0336d.o: ../../JuceLibraryCode/include_melatonin_inspector.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_melatonin_inspector.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/juce_LinuxSubprocessHelperBinaryData_e1db7dc.o: pre_build/juce_LinuxSubprocessHelperBinaryData.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling juce_LinuxSubprocessHelperBinaryData.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_SHARED_CODE) $(JUCE_CFLAGS_SHARED_CODE) -o "$@" -c "$<"

$(JUCE_OBJDIR)/juce_VST3ManifestHelper_d62bf7a7.o: ~/JUCE/modules/juce_audio_plugin_client/VST3/juce_VST3ManifestHelper.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling juce_VST3ManifestHelper.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_VST3_MANIFEST_HELPER) $(JUCE_CFLAGS_VST3_MANIFEST_HELPER) -o "$@" -c "$<"

$(JUCE_OBJDIR)/execinfo.cmd:
	-$(V_AT)mkdir -p $(@D)
	-@if [ -z "$(V_AT)" ]; then echo "Checking if we need to link libexecinfo"; fi
	$(V_AT)printf "int main() { return 0; }" | $(CXX) -x c++ -o $(@D)/execinfo.x -lexecinfo - >/dev/null 2>&1 && printf -- "-lexecinfo" > "$@" || touch "$@"

$(JUCE_OBJDIR)/cxxfs.cmd:
	-$(V_AT)mkdir -p $(@D)
	-@if [ -z "$(V_AT)" ]; then echo "Checking if we need to link stdc++fs"; fi
	$(V_AT)printf "int main() { return 0; }" | $(CXX) -x c++ -o $(@D)/cxxfs.x -lstdc++fs - >/dev/null 2>&1 && printf -- "-lstdc++fs" > "$@" || touch "$@"

$(JUCE_OBJDIR)/juce_LinuxSubprocessHelper_d0e1814f.o: make_helpers/juce_LinuxSubprocessHelper.cpp
	-$(V_AT)mkdir -p $(JUCE_OBJDIR)
	@echo "Compiling make_helpers/juce_LinuxSubprocessHelper.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) -o "$(JUCE_OBJDIR)/juce_LinuxSubprocessHelper_d0e1814f.o" -c "$<" $(TARGET_ARCH)

$(JUCE_BINDIR)/juce_linux_subprocess_helper: $(JUCE_OBJDIR)/juce_LinuxSubprocessHelper_d0e1814f.o
	-$(V_AT)mkdir -p $(JUCE_BINDIR)
	@echo "Linking $(JUCE_BINDIR)/juce_linux_subprocess_helper"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) -o "$(JUCE_BINDIR)/juce_linux_subprocess_helper" "$(JUCE_OBJDIR)/juce_LinuxSubprocessHelper_d0e1814f.o" $(JUCE_LDFLAGS) $(TARGET_ARCH)

$(JUCE_OBJDIR)/juce_SimpleBinaryBuilder_18cb8206.o: make_helpers/juce_SimpleBinaryBuilder.cpp
	-$(V_AT)mkdir -p $(JUCE_OBJDIR)
	@echo "Compiling make_helpers/juce_SimpleBinaryBuilder.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) -o "$(JUCE_OBJDIR)/juce_SimpleBinaryBuilder_18cb8206.o" -c "$<" $(TARGET_ARCH)

$(JUCE_BINDIR)/juce_simple_binary_builder: $(JUCE_OBJDIR)/juce_SimpleBinaryBuilder_18cb8206.o
	-$(V_AT)mkdir -p $(JUCE_BINDIR)
	@echo "Linking $(JUCE_BINDIR)/juce_simple_binary_builder"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) -o "$(JUCE_BINDIR)/juce_simple_binary_builder" "$(JUCE_OBJDIR)/juce_SimpleBinaryBuilder_18cb8206.o" $(JUCE_LDFLAGS) $(TARGET_ARCH)

pre_build/juce_LinuxSubprocessHelperBinaryData.cpp: $(JUCE_BINDIR)/juce_linux_subprocess_helper $(JUCE_BINDIR)/juce_simple_binary_builder
	$(V_AT)"$(JUCE_BINDIR)/juce_simple_binary_builder" "$(JUCE_BINDIR)/juce_linux_subprocess_helper" "pre_build" "juce_LinuxSubprocessHelperBinaryData" LinuxSubprocessHelperBinaryData

clean:
	@echo Cleaning NeuroCore
	$(V_AT)$(CLEANCMD)

strip:
	@echo Stripping NeuroCore
	-$(V_AT)$(STRIP) --strip-unneeded $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3)
	-$(V_AT)$(STRIP) --strip-unneeded $(JUCE_OUTDIR)/$(JUCE_TARGET_STANDALONE_PLUGIN)
	-$(V_AT)$(STRIP) --strip-unneeded $(JUCE_OUTDIR)/$(JUCE_TARGET_VST3_MANIFEST_HELPER)

-include $(OBJECTS_VST3:%.o=%.d)
-include $(OBJECTS_STANDALONE_PLUGIN:%.o=%.d)
-include $(OBJECTS_SHARED_CODE:%.o=%.d)
-include $(OBJECTS_VST3_MANIFEST_HELPER:%.o=%.d)
