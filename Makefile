
TARGET ?= bicycle

BUILD_DIR ?= ./build
OBJS :=
INCS :=

# INC_FLAGS and CPPFLAGS must be recursive (=) not simple (:=)

INC_FLAGS = $(addprefix -I,$(INCS))

CPPFLAGS = $(INC_FLAGS)
CPPFLAGS += -MMD -MP
CPPFLAGS += -DRPI -DARDUINO=100
CPPFLAGS += -fdata-sections -ffunction-sections



MKDIR_P ?= mkdir -p

define __build_objects =

$(info build_objects $(1))
$(info --from: sources in $(2))
$(info --files: $(3))

OBJS += $(addprefix $(BUILD_DIR)/$(1)/,$(3:%=%.o))
INCS += $(2)

# assembly
$(BUILD_DIR)/$(1)/%.s.o: $(2)/%.s
	$(MKDIR_P) $(BUILD_DIR)/$(1)
	$(AS) $(ASFLAGS) -c $$< -o $$@

# c source
$(BUILD_DIR)/$(1)/%.c.o: $(2)/%.c
	$(MKDIR_P) $(BUILD_DIR)/$(1)
	$(CC) $$(CPPFLAGS) $(CFLAGS) -c $$< -o $$@

# c++ source
$(BUILD_DIR)/$(1)/%.cpp.o: $(2)/%.cpp
	$(MKDIR_P) $(BUILD_DIR)/$(1)
	$(CXX) $$(CPPFLAGS) $(CXXFLAGS) -c $$< -o $$@

endef

define build_objects =
$(call __build_objects,$(strip $(1)),$(strip $(2)),$(3))
endef


$(eval $(call build_objects,\
  rpi,\
  rpi,\
  bicycle.cpp))

$(eval $(call build_objects,\
  common,\
  common,\
  cell.cpp display.cpp looper.cpp))

$(eval $(call build_objects,\
  ClearUI,\
  ../ClearUI/src,\
  ClearUI_Display.cpp ClearUI_Field.cpp ClearUI_Layout.cpp))

$(eval $(call build_objects,\
  Adafruit-GFX-Library,\
  ../ext/Adafruit-GFX-Library,\
  Adafruit_GFX.cpp))

$(eval $(call build_objects,\
  Adafruit_SSD1306,\
  ../ext/Adafruit_SSD1306,\
  Adafruit_SSD1306.cpp))

$(eval $(call build_objects,\
  lard,\
  lard,\
  main.cpp))


LIBS := stdc++
LDFLAGS := -Wl,--gc-sections $(addprefix -l,$(LIBS))

$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) 2>&1 | tee $(BUILD_DIR)/link_out
	@echo === missing ===
	@grep 'undefined reference to' $(BUILD_DIR)/link_out \
	| sed -e 's/^.*undefined reference to //' \
	| sort -u



.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)


DEPS := $(OBJS:.o=.d)

-include $(DEPS)


