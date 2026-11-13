// Sanitizer.h - layer 4 of the anti ban.
#pragma once

namespace mizze {
namespace sanitize {

// No debugger may attach to the game while it runs.
void DenyDebuggerAttach();

// Removes the loader variables from the environment of the process.
void CleanEnvironment();

// Deletes the files the toolkit wrote.
void RemoveTraces();

// Marks every open descriptor close on exec.
void SealInheritedDescriptors();

}  // namespace sanitize
}  // namespace mizze
