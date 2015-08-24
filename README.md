# putty-wcf

This is a PuTTY fork. For the real PuTTY, see http://www.chiark.greenend.org.uk/~sgtatham/putty/

PuTTY with Windows Clipboard, XTerm New, and Foxpro for Unix support.

Major Features:
* Compiles in Pelles C for easy development
* vt100 print to Windows clipboard
* XTerm New keyboard, now the default
* Per window invert caps lock
* Foxpro for Unix key modifiers
* Portable PuTTY

I use a number of programs in a terminal that use lots of function keys like Midnight Commander. I also use PuTTY and find that no matter how bad PuTTY seems to be, all other terminal emulators are worse. I could name names, but that would be all of them on the list at [Wikipedia: List of terminal emulators](https://en.wikipedia.org/wiki/List_of_terminal_emulators) The biggest problem is that they purloin many of the function keys for local use. Hit F3 to do something useful, and hmmm, it seems F3 is used for search in XFCETerm. Hit Ctrl+PgUp, and that does something with the scroll back buffer which isn't even enabled. PuTTY looks like it transmits a lot of keys with shift and control but it turns out that many are duplicates. The only PuTTY keyboard emulation the least bit useful is SCO which transmits a full set of function keys. Unfortunately all you get in SCO is the function keys. The shift and control with the movement keys is a no show.

What I want is something really simple. My keyboard needs to work like a PC keyboard at all times. That means all keys with Shift, Control, & Alt get sent into the terminal. This includes all the function keys, the directional keys, and the keypad keys. Every key, every shift state, no exceptions! No matter how bad PuTTY seems with keys, PuTTY sends more keys than any other terminal emulator.

While exploring ways to get all function and keypad keys working in Foxpro for Unix, I found many many hacks for PuTTY but most were not substantial and the ones that were substantial were only piecemeal. One person's hack give you more function keys. Someone else's hack gives you clipboard support. Nowhere to be found is all the things you want in one PuTTY. [Zoc](http://www.emtec.com/zoc/) has a decent function key mapper but it looked like a big chore to get them all to work, and there's no way to get print to Windows Clipboard support in Rexx. What I need is way too complex to implement in a macro editor.

There are many mods to PuTTY but most are visual improvements and useless. Simon Tatham seems to know that once you get PuTTY set up you don't interact with the configuration screen very much so there's no reason to make UI elements better. Let's see what useful things people have done with PuTTY. 

Diomidis Spinellis made a Windows Clipboard patch for PuTTY 0.53b. GitHub-ootput adapted this patch to PuTTY 0.60.

Robert Skopalik made begPuTTY which adds some shift states to some directional keys.

Troy added a few shift states to the directional keys. https://github.com/troydm/putty-modified

There are some wrapper programs that copy back and forth between PuTTY's registry entries to an INI file to make PuTTY portable.

Another person claims to have coded up the Xterm New keyboard in just a few lines but so far as I can tell neither source nor executable was ever published. I've done it and I can tell you it's not just a few lines. It's a few lines plus a lot of code and rearranging to get those few lines to work, and even more lines for unit testing to demonstrate that the few lines produce all the right escape sequences.

Those are some must have features. What I really need is all those in one package, and judging from comments around the Interwebs plenty of other users need it too. PuTTY seemed a good starting point. PuTTY steals less keys than any other terminal. PuTTY already has a few published patches I can start with. PuTTY is written entirely in C. And after a bit of fiddling, PuTTY builds in [Pelles C](http://www.smorgasbordet.com/pellesc/), the only C compiler with a useful IDE debugger.

I did it all and is it nice. Now that the best terminal has a keyboard emulation from this century the other terminal programs are of no interest. Midnight Commander and mcedit don't suck once you get behind a good keyboard emulation.

And for anyone who asks, Xterm New does not fix the problem where the arrow keys cannot be made to always work in all applications. This is only indirectly a PuTTY problem because Simon Tatham refuses to add Xterm New. Maybe if we could all get on a single keyboard emulation standard the app designers wouldn't need to use half baked techniques to get all the special keys to work. 

Why PuTTY and not a random terminal that already does XTerm New? There are many subtle things that PuTTY gets right that all other terminals get wrong like color, indirection, and per server settings. PuTTY is the least bad of all the terminals. The only thing that PuTTY gets wrong is keyboard emulation. It's easier to suffer with a bad keyboard than to suffer through what all the other terminals get wrong.

Why isn't this in PuTTY already? I can't get through to SGT. I've rewritten most of the Windows keyboard handling code. What could possibly go wrong? There might be a bug or a lost feature somewhere. I've tested version 0.63 for 1.5 years and it's time for others to try to break it. Some things I've added really need configuration options. Some things I've added are controversial. 

For more information, see the detailed [README](./putty065wcf/src/windows/POCC/readme.txt)
