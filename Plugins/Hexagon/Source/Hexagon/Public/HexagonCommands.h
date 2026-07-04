// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#if WITH_EDITOR
/** Register all hex.* console commands (called on PostEngineInit). */
void RegisterHexCommands();
/** Unregister all hex.* console commands (called on ShutdownModule). */
void UnregisterHexCommands();
#endif
