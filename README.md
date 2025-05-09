# ultrakrill
Simple sidescrolling game running on an Arduino UNO with a character LCD inspired by [ULTRAKILL](https://en.wikipedia.org/wiki/Ultrakill).

## Gameplay
You automatically move to the right, and must avoid obstacles and enemies to survive as long as possible. Your health slowly drains over time, and must be restored by consuming blood. There are 9 layers, each of which has you moving faster and encountering more patterns than the previous.

### Level elements
- Blocks a limited number of shots and damages you when ran into.
- Looks cool but its only purpose is to damage you.
- One of the two kinds of enemies. Doesn't do much, but is a good source of blood.
- The other kind of enemy. Occasionally shoots balls of Hell Energy at you.
- Press *fire* in the main menu to start playing. The intro can be fast-forwarded with *down*, or skipped entirely with *fire*.

### Controls
Use *up* to jump, and *down* to slide.
Tap *fire* to shoot, or hold it down to charge up a stronger shot.
Tapping *fire* while an enemy projectile is right in front of you parries it and sends it flying back.

You can press *down* and *up* to view statistics on the death screen. These are also sent over serial encoded as JSON upon death.

## Gameplay tips
- Jumping quickly after ending a slide lets you fly through the air quickly.
- Explosive shots spill more blood.
- Imps contain more blood than filths, making up for their increased health.
- Blood evaporates quickly, so allowing enemies closer to you is a good idea.
- You can walk and even slide on top of walls.

## Hardware setup
Parts:
- Arduino UNO R3
- Character LCD (16x2 recommended, game width can be adjusted by tweaking `LCD_WIDTH`)
- Potentiometer for adjusting LCD contrast (optional but highly recommended)
- 3 buttons

The pins for everything are near the top of the source code. Buttons should be pulled HIGH. The LCD's RW pin is not used, so it should be tied LOW.

## Compiling
The `ultrakrill` directory is an Arduino sketch, which can be compiled and uploaded via arduino-cli or a similar tool.

The emulator made by the makefile used to be a working program that used SDL to fake Arduino functions for the program for running on a computer, but it hasn't been updated to support PROGMEM stuff yet.
