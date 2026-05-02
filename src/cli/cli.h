#ifndef _DS1EDIT_CLI_H_
#define _DS1EDIT_CLI_H_

// CLI mode for ds1edit. When the user invokes `ds1edit.exe <verb> [args...]`
// where <verb> is one of the known CLI verbs, the editor runs in headless
// mode: it does the minimum init needed for the requested verb, executes
// it, and exits without ever creating an Allegro display.
//
// Verbs (designed in the planning chat, see docs/project_reports for the
// authoritative log):
//
//   list-mpqs         show which MPQ slots opened, which didn't
//   probe <path>      ask the MPQ chain for a single virtual path
//   probe-cof <token> <mode> <weapon>
//                     build the COF path for a tuple, load + parse, dump
//                     layer count + direction count
//   list-tokens <category>
//                     dump every token compose_index sees in <category>
//                     (chars / monsters / npcs / objects)
//   list-presets      dump the parsed [char_mode_presets] and
//                     [char_weapon_presets] sections
//   export            alias for `export-compose`
//   export-compose    one-shot compose-mode iteration with CLI selectors
//   export-raw        one-shot raw-frame export (DCC/DC6/DT1) with CLI
//                     selectors
//
// Exit codes (consistent across all verbs):
//   0  clean run, at least one file produced (export verbs) or success
//   1  some failures occurred but the run completed
//   2  nothing was produced (no MPQs, bad selector, no matches)
//   3  bad arguments

// Returns 1 if argv[1] looks like a CLI verb (caller should run the CLI
// path); 0 otherwise (caller should run the GUI path). Does NOT execute
// the verb; just classifies argv.
int cli_is_verb(int argc, char **argv);

// Run the CLI flow end to end. Caller must have done the bare-minimum
// Allegro init (al_init + image/font/primitives addons) by the time
// this is called; the CLI does its own ini load + MPQ open from there.
//
// Returns the process exit code (0..3 per the table above).
int cli_run(int argc, char **argv);

#endif
