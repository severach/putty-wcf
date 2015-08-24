Why compile in Pelles C?

Pelles is free, installs in about 5 minutes, and has a debugger second to none. It makes improving PuTTY easy.

How to compile in Pelles C.

Pelles C only compiles PuTTY. Other tools need some minor fixes to be compatible with the Pelles compiler.

1) Install Pelles C and SDK.
2) Unpack PuTTY source.
3) Copy wcf folder into source, overwriting as necesary.
4) Run mlink.bat in POCC to create link.
5) Click on .ppj file to start Pelles IDE.

Fixes:
  All fixes are for Windows only. Quick fix features can be disabled at compile time. PuTTY compiles 
    in Pelles-C which is why I can develop all this in only a few weeks. Thanks to Pelles-C anyone 
    can download the compiler, the source, apply my
    file changes, and have *A WORKING* compile in less than 30 minutes. 
    All changes are laid out to be easily
    visible in the diff editor inside Total Commander with "Ignore frequent lines" checked. 
    The output from any other diff tool is likely to be much less useful.

  New printer option: (Windows Clipboard). This printer saves with settings correctly.

  For existing keyboard emulations all unsupported Ctrl+Shift+Alt function and keypad 
    keys have been removed. Unsupported shift states are those that send the same code
    as the same key with a different shift state or the same code as it would in another
    emulation. Example: Alt+F2 and F2 send the same code, Alt+F2 now sends nothing.
    No valid keys have been removed, only keys that were duplicate or incorrect.
    This may cause retraining issues as people may have learned to use the invalid keys.
    I could have extended the various standards but why bother. It would have created 
    more schisms and all those keyboard emulations suck anyways. We've already got a 
    good standard in Xterm New and that's what we need to use.

  Ctrl+TAB sends ESC+TAB for Tab Completion in mc. This should be Alt-Tab but Windows
    doesn't allow applications to use it.

  Shift no longer switches arrow keys out of application cursor keys mode. Only Ctrl does. 
    If various Shift+Arrows don't send a unique code then they send nothing.

  Xterm New keyboard emulation. XTerm New disables some of the Shift PgDn PgUp Delete shortcuts
    for history and paste as these keys are needed for the terminal. Alt+F4 also transmits to the
    terminal if you switch it off in the PuTTY options as a close key. That should be all of them.
    Like with Xterm R6, application keypad mode is disabled in Xterm New. I couldn't find a single application that
    accepted the extended codes and none of the Linux Xterm emulators send application keypad
    codes. I want Xterm New to function properly with the least amount of
    configuration changes, just as now the horrible emulation ESC[n~ is supported everywhere 
    just because it's the default in PuTTY.
    A horrible emulation is defined as one that does not send most shift states for function
    and movement keys. Without these shift states we are left with lame shortcuts in 
    Midnight Commander and a bunch of good keys that don't do anything except scribble escape
    codes all over. 
    Alt+Numeric Keypad does not produce movement escape sequences in preference to Alt+000
    keycode generation, a feature not available in XFCETerm where Alt+Numeric Keypad keys do produce
    codes.

  Function & Keypad Keys: the code for Xterm New is programmed to produce Meta codes 9-16. That's 
    all 16 shift states for every function and movement key on the keyboard and I've disabled all 
    the local key thieves. For now the Meta key is the left Windows key but because it's used
    by Windows it behaves a little funny. It takes some practice to use that key without
    activating the Start Menu or the many secret actions. Not every shift state is available because 
    some are used by Windows. To prevent this from becoming a standard before SGT and others
    can comment on the viability of the VK_LWIN as Meta, Meta 9-16 is disabled in standard 
    compiles but is very easy to enable if you'd like to experiment. 

  Save in the reconfiguration dialog no longer overwrites Host, Port, or Connection Type for 
    an existing connection. All settings are overwritten for new connections and for saves 
    initiated by the new connection dialog. This allows you to copy common settings across 
    many different servers.

  The system menu provides a per window Invert Caps Lock. This allows you to operate a terminal
    as if Caps Lock is on without having the real Caps Lock on and typing capital letters in 
    every application. This is not saved in the configuration.
    If it were it would need to be loaded after logon, something non trivial to detect. 
    In my case I open multiple windows to the same host, some need caps lock on and some need it
    off. Saving would not help because I'd need to turn it off as many times as I'd need to 
    turn it on.
    Per Window Caps lock is also settable by escape code. "CSI ? 1064 h" turns caps lock on and 
    "CSI ? 1064 l" turns caps lock off. This is complementary to the system menu setting. Unfortunately
    in my application the caps lock off code is often ignored and I don't know why.

  Special keyboard emulation for Foxpro Unix. Foxpro for Unix provides Control and Shift modifiers
    to enable many keys normally unavilable in a terminal. Unfortunately using the sequences by hand is 
    difficult, particularly if you also use Foxpro for DOS where all keys work naturally. 
    The special keyboard emulation attaches the modifiers to all necessary keys so most keys
    work just as they do in DOS. 
    Alt+Menu letters in the default system menu are emulated via F10. More could be made to work.
    Ctrl+Shift Letters+Numbers+Symbols all work. You can check what works with with the Shift-F10
    macro editor.
    All Control and Shift directional keys work for editing and other functions, NumLock on or off. 
    I'd implement full Alt support if someone can find an Alt code. 
    Foxpro keyboard emulation is enabled by "CSI ? 1063 h" and disabled with "CSI ? 1063 l". Enabling via
    escape code makes the special key sequences work only in Foxpro so they don't interfere with 
    other programs that want standard sequences. Foxpro keyboard emulation enables full 
    movement and function keys in every keyboard emulation.

  PuTTY is portable. Just create PUTTY.INI in the same folder as PUTTY. If you have bunch of sessions in
    your registry you'd like to have in your INI file create PUTTY.INI with this contents.
[Settings]
LocalReg=import
    Later if you have a fine bunch of sessions in your INI file that you want in your registry set
LocalReg=export
    and the standard registry location will be filled with your INI sessions and host keys.
    After any action LocalReg automatically changes to none so it won't happen again. When run from
    a CDROM or other read only media PuTTY will be able to load settings but save attempts will be 
    silently ignored. You can set ReadOnly on PUTTY.INI if you'd like to prevent users from saving settings.
    If you rename your PUTTY.EXE to another name like KITTEH.EXE, your INI and RND file will change
    also: KITTEH.INI, KITTEH.RND

  PuTTY no longer writes all session values to REG or INI. Most default values are not written. This eases
    hand editing the INI or REG by eliminating the clutter of rare default values.

  Future Feature: I considered adding centralized management. The idea would be that a setting in the INI
    or REG would refer to a server, and if available, settings would be read from the referred file instead
    of local. The referred file might contain a directive for updating the local copy. In this way company
    wide changes would only need a change on the server then have everyone close and reopen PuTTY. I didn't
    implement this because I suspect my ideas of how it should work aren't very good. Maybe others have good
    ideas.
    In particular I want:
    Use server settings if available, otherwise local settings.
    Update a single session and host key from the server.
    Update all sessions and host keys from the server.
    All terminals switch from REG to INI or INI to REG.

  Copy Modes: Did your program crash and now your terminal is foobar? Maybe your arrows 
    don't work. Seeing only linedraw characters? Want to learn about escape codes?
    Trying to debug PuTTY or your curses application? Stuck in print mode wondering how
    to get out? Why is the mouse scribbling escape codes? Am I in UTF-8? What mode does my favorite
    application use? Does vi change modes between escape to insert?
    Select Copy Modes from the PuTTY menu then paste into Notepad or your favorite text editor.
    No need to bang on keys to figure out the mode, which doesn't always work because sometimes
    banging on keys changes the mode. Now you can measure without changing the outcome. There are a lot
    more settings that need to be shown. I only implemented the ones I know something about. Hopefully
    SGT will dig in and add a bunch more.

  I think I've fixed the disaster known as Application Keypad Mode. It should send the
    right keys in the right keyboard modes. SCO mode no longer sends application keys for dot & 0-9
    which should make the SCO keyboard less useless.
    For some reason xTerm implements the 2-16 Sun shift states on Keypad Enter, Plus, Minus, *, and /
    to match the 16 shift states on the remaining movement keys.
    I see no reason to duplicate this, especially because xterm steals many of the shift states
    for keyboard shortcuts like shift plus for zoom up. Support isn't consistent because Ctrl Shift 
    plus is ...um, zoom down??? XFCETerm Home & End sends the wrong keys so I won't be sending those keys either.
    As with function keys, superflous shift states have been eliminated. 

  Alt-[ and Alt-O are not available as these start most of the keyboard codes.

  TODO: Set CodePage by escape code. PuTTY Properties fix.

  Bug: If you save a session with XTerm New and load a different version of PuTTY that doesn't have at least as many
       keyboard emulations you will get an assert error. Just accept it and 
       change the keyboard emulation. 

  Bug Fix: PuTTY is supposed to send nothing when the ^E answerback is blank. 
     It does send something though I don't know what. This bug is fixed.

  The following are bugs that might be related to my custom compile.

  Bug: On occasion I lose access to the control key. ^C doesn't work. I end up closing the terminal.

  Bug: On occasion PuTTY will freeze. It's not locked up, just stuck in some loop. I first noticed this when
     debugging. The debugger would stop, then I would start it again and PuTTY would be stuck, unable
     to send or receive characters. I suspect there are many things in PuTTY on timers that expect things
     to happen fast. When they happen slow because of a pause in a debugger the timers get off kilter and
     millisecond delay times turn into one day delay times. If SGT would use real compilers with real 
     debugging capability like Pelles-C instead of the crap compilers they use now this bug
     would be found and fixed fast.

  Things users should check for:

  For XTerm New I had to rearrange a lot of code. I've checked as many keys as I can
    figure out but I can't check them all. Some might be wrong or missing.

  Maybe I've been too draconian eliminating the superflous shift states on the keys.
    They can be added back if the pain is too great. 

  Though I've left all the function keys alone for the original 6 emulations (Tilde-SCO) 
    so that all your customized termcaps continue to work, function key 
    support is still a bad situation and should change. There appear to be mistakes
    in both PuTTY and termcap files. It would be nice if function keys 
    were completely supported without users hacking termcap/terminfo.
    For this to happen termcap/terminfo needs to be fixed and PuTTY needs to be fixed.
    Here's what I think should happen.
http://aperiodic.net/phil/archives/Geekery/term-function-keys.html
    Tilde, Linux Console, and SCO are fine. Trouble is we don't use any of these.
    Tilde may be the PuTTY default but very few xterm function key listings match it so
    we use Tilde with only accidental function key support. 
    Terminfo vt102, vt220, & vt420 should be expanded to have the full contingent of
    vt220 + F5 from X11R6. It's OK to define extra keys. vt220 users will hit the blank
    spot between F4 and F6 and won't see the hackery behind the scenes that allow our
    PC keyboards to be better supported. Phil Gold's list seems to most closely match 
    what termfiles expect.
http://aperiodic.net/phil/archives/Geekery/term-function-keys.html
    PuTTY VT100+ fky codes should be changed to match.
    With XTerm New, maybe none of this matters and we can push all those shoddy emulations
    into the dust bin of history.
  With that in mind I've changed the default keyboard for all new connections to 
    Xterm New. It can't be any worse than Tilde which doesn't even offer a functional
    NumLock.

Here are my notes while exploring Foxpro for Unix ^Y characters.
  Remove code for CTRL+KP5, it doesn't do anything
  Keypad keys do not work in Fox. (Application mode)
  ^ESC not available in Windows
  ^TAB
  ^6 = CTRL+CARET
  ^- = CTRL+HYPHEN
  ^_ = SHIFT+CTRL+HYPHEN
  ^BACKSPACE
  ^[ = Escape
  ^] = CTRL+RBRACKET (impossible as this would take the place of refresh screen)
  ^\ = CTRL+BACKSLASH

  TODO: Record ^Y keys
  ^Y 1-0: F1-F10
  ^Y-=~`!@#$%^&*()_+\|{}[]:";'<>,.?/ QTYIPGHK (These keys are ^same)
  ^YBACKSPACE = Left Arrow
  ^YW Ctrl+F8
  ^YE Ctrl+End
  ^YR PgUp
  ^YU Ctrl+F10
  ^YO Ctrl+F1
  ^YA Ctrl+Left Arrow
  ^YS Home
  ^YD DownArrow
  ^YF Ctrl RightArrow
  ^YJ Ctrl Spacebar
  ^YL Ctrl+F9
  ^YM Ctrl+M
  ^YN Ctrl+F2
  ^YB Ctrl+Home
  ^YV Ctrl+F7
  ^YC PgDn
  ^YX (Invalid Key, does something along with a 3rd key)

