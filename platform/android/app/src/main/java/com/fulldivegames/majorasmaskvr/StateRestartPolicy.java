package com.fulldivegames.majorasmaskvr;

/** Pure lifecycle policy. Never launch before both commit and original process death. */
final class StateRestartPolicy {
    enum Step { WAIT, LAUNCH, STOP }
    private final long started;
    private long committed = -1;
    private boolean finished;

    StateRestartPolicy(long now) { started = now; }
    boolean commit(long now) {
        if (finished || committed >= 0 || now - started >= 7000) return false;
        committed = now;
        return true;
    }
    void cancel() { finished = true; }
    Step poll(long now, boolean parentAlive) {
        if (finished) return Step.STOP;
        if (committed < 0) {
            if (now - started < 7000) return Step.WAIT;
        } else {
            if (!parentAlive) { finished = true; return Step.LAUNCH; }
            if (now - committed < 60000) return Step.WAIT;
        }
        finished = true;
        return Step.STOP;
    }
}
