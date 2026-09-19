# Telemetry

Nothing is sent anywhere. `[/Script/Projects.ProjectApexSettings]` in `Config/DefaultGame.ini` is a placeholder
so that turning analytics on later is a configuration change, and the `UserActivityTracking` plugin is
explicitly disabled in `FantasyCardBattle.uproject`.

If a backend ever exists, these are the events worth having - all of them are already computed in
`FFCBMatchState` / `FFCBRoundResult`, so collecting them is a sink on the existing structs, not new
instrumentation in the rules path:

| Event | Payload | Why it is the one worth having |
|-------|---------|--------------------------------|
| `match_started` | seed, hand size, tie rule, difficulty, pool size | lets every other number be normalised |
| `round_resolved` | outcome, margin, whether an ability fired, pot size | the only way to see whether abilities feel good in play rather than in the duel matrix |
| `match_finished` | winner seat, rounds, score, end reason | game-length and turn-rate validation against Docs/Balance.md |
| `ai_reasoning` | chosen move EV versus best EV (debug builds only) | catches a difficulty tier drifting into "random" |

Two rules for any of it:

1. **No gameplay code reads telemetry.** If a decision needs the data, the data belongs in the match state.
2. **Seed, not history.** The seed reproduces a game exactly, so a bug report needs the seed and the config;
   storing full hands would be a privacy cost with no debugging benefit. See `Docs/Setup.md` for the replay
   path.
