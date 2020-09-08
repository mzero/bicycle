
TARGET ?= bicycle

BUILD_DIR ?= ./build
SRCS :=
OBJS :=
INCS :=


APP_SRC_DIR := rpi
APP_OBJ_DIR := $(BUILD_DIR)/rpi
APP_SRC_FILES := bicycle.cpp
SRCS += $(addprefix $(APP_SRC_DIR)/,$(APP_SRC_FILES))
OBJS += $(addprefix $(APP_OBJ_DIR)/,$(APP_SRC_FILES:%=%.o))
INCS += $(APP_SRC_DIR)

COM_SRC_DIR := common
COM_OBJ_DIR := $(BUILD_DIR)/common
COM_SRC_FILES := cell.cpp display.cpp looper.cpp
SRCS += $(addprefix $(COM_SRC_DIR)/,$(COM_SRC_FILES))
OBJS += $(addprefix $(COM_OBJ_DIR)/,$(COM_SRC_FILES:%=%.o))
INCS += $(COM_SRC_DIR)

CUI_SRC_DIR := ../ClearUI/src
CUI_OBJ_DIR := $(BUILD_DIR)/ClearUI
CUI_SRC_FILES := ClearUI_Display.cpp ClearUI_Field.cpp ClearUI_Layout.cpp
SRCS += $(addprefix $(CUI_SRC_DIR)/,$(CUI_SRC_FILES))
OBJS += $(addprefix $(CUI_OBJ_DIR)/,$(CUI_SRC_FILES:%=%.o))
INCS += $(CUI_SRC_DIR)

GFX_SRC_DIR := ../ext/Adafruit-GFX-Library
GFX_OBJ_DIR := $(BUILD_DIR)/Adafruit-GFX-Library
GFX_SRC_FILES := Adafruit_GFX.cpp
SRCS += $(addprefix $(GFX_SRC_DIR)/,$(GFX_SRC_FILES))
OBJS += $(addprefix $(GFX_OBJ_DIR)/,$(GFX_SRC_FILES:%=%.o))
INCS += $(GFX_SRC_DIR)

SSD_SRC_DIR := ../ext/Adafruit_SSD1306
SSD_OBJ_DIR := $(BUILD_DIR)/Adafruit_SSD1306
SSD_SRC_FILES := Adafruit_SSD1306.cpp
SRCS += $(addprefix $(SSD_SRC_DIR)/,$(SSD_SRC_FILES))
OBJS += $(addprefix $(SSD_OBJ_DIR)/,$(SSD_SRC_FILES:%=%.o))
INCS += $(SSD_SRC_DIR)

LARD_SRC_DIR := lard
LARD_OBJ_DIR := $(BUILD_DIR)/lard
LARD_SRC_FILES := main.cpp
SRCS += $(addprefix $(LARD_SRC_DIR)/,$(LARD_SRC_FILES))
OBJS += $(addprefix $(LARD_OBJ_DIR)/,$(LARD_SRC_FILES:%=%.o))
INCS += $(LARD_SRC_DIR)


DEPS := $(OBJS:.o=.d)

INC_FLAGS = $(addprefix -I,$(INCS))

CPPFLAGS := $(INC_FLAGS)
CPPFLAGS += -MMD -MP
CPPFLAGS += -DRPI -DARDUINO=100
CPPFLAGS += -fdata-sections -ffunction-sections

LDFLAGS := -Wl,--gc-sections


$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)


MKDIR_P ?= mkdir -p

define make_rules =
$(info make_rules for $(1) from $(2))

# assembly
$(1)/%.s.o: $(2)/%.s
	$(MKDIR_P) $(1)
	$(AS) $(ASFLAGS) -c $$< -o $$@

# c source
$(1)/%.c.o: $(2)/%.c
	$(MKDIR_P) $(1)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $$< -o $$@

# c++ source
$(1)/%.cpp.o: $(2)/%.cpp
	$(MKDIR_P) $(1)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $$< -o $$@
endef


$(eval $(call make_rules,$(APP_OBJ_DIR),$(APP_SRC_DIR)))
$(eval $(call make_rules,$(COM_OBJ_DIR),$(COM_SRC_DIR)))
$(eval $(call make_rules,$(CUI_OBJ_DIR),$(CUI_SRC_DIR)))
$(eval $(call make_rules,$(GFX_OBJ_DIR),$(GFX_SRC_DIR)))
$(eval $(call make_rules,$(SSD_OBJ_DIR),$(SSD_SRC_DIR)))
$(eval $(call make_rules,$(LARD_OBJ_DIR),$(LARD_SRC_DIR)))



.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)


