# Project:   SF3KtoProT

# Tools
CC = gcc
Link = gcc

# Toolflags:
CCCommonFlags = -c -Wall -Wextra -pedantic -std=c99 -MMD -MP -MF $*.d -o $@
CCFlags = $(CCCommonFlags) -DNDEBUG -O3
CCDebugFlags = $(CCCommonFlags) -g -DDEBUG_OUTPUT
LinkCommonFlags = -o $@
LinkFlags = $(LinkCommonFlags) $(addprefix -l,$(ReleaseLibs))
LinkDebugFlags = $(LinkCommonFlags) $(addprefix -l,$(DebugLibs))

include MakeCommon

DebugObjects = $(addsuffix .debug,$(ObjectList))
ReleaseObjects = $(addsuffix .o,$(ObjectList))
DebugLibs = CBUtildbg Streamdbg GKeydbg
ReleaseLibs = CBUtil Stream GKey 

# Final targets:
all: SF3KtoProT SF3KtoProTD 

SF3KtoProT: $(ReleaseObjects)
	$(Link) $(ReleaseObjects) $(LinkFlags)

SF3KtoProTD: $(DebugObjects)
	$(Link) $(DebugObjects) $(LinkDebugFlags)

# User-editable dependencies:
.SUFFIXES: .o .c .debug
.c.debug:
	$(CC) $(CCDebugFlags) $<
.c.o:
	$(CC) $(CCFlags) $<

# Static dependencies:

# Dynamic dependencies:
# These files are generated during compilation to track C header #includes.
# It's not an error if they don't exist.
-include $(addsuffix .d,$(ObjectList))
-include $(addsuffix D.d,$(ObjectList))
