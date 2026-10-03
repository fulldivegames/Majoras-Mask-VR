#pragma once
#include <string>

// Call on the game thread, only after the pending restore and preferences are
// durable. Success means a separate helper has acknowledged the clean restart
// and Window::Close was requested. Failure leaves the game running; the staged
// restore remains available for an ordinary manual restart.
bool MMVR_RequestStateRestart(std::string& error);
