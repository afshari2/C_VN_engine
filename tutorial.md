# Story Script Tutorial

The game uses one active story file:

```txt
assets/story.txt
```

Put the whole story in that file. You can make it as large as you need by using
labels, jumps, choices, and variables.

## Basic Structure

Each command starts on a new line.

```txt
# Lines starting with # are comments.

SET himari_affection 0

BG assets/backgrounds/dorm_entrance.png
SPRITE assets/sprites/cat_hero.png center

SAY Narrator Rain taps against the front steps.
SAY Himari Oh no. Were you out here all night?

END
```

Blank lines are allowed. Lines starting with `#` are ignored.

## Backgrounds

Use `BG` to change the background image.

```txt
BG assets/backgrounds/hallway.png
```

Backgrounds usually live in:

```txt
assets/backgrounds/
```

## Sprites

Use `SPRITE` to show a character sprite.

```txt
SPRITE assets/sprites/emi_smile.png center
```

Supported positions:

```txt
left
center
right
```

Only one sprite is shown at a time.

## Dialogue

Use `SAY` for narration or character dialogue.

```txt
SAY Narrator The hallway is warm and quiet.
SAY Himari You need a name. Tama. That fits, right?
```

The first word after `SAY` is the speaker name. Everything after that is the
text shown in the dialogue box.

Use `SAYN` for narration with no speaker prefix.

```txt
SAYN The hallway is warm and quiet.
```

`SAYN` displays only the text, without `[Narrator]`.

## Choices

Choices start with `CHOICE`, use up to 4 `OPTION` lines, and end with
`ENDCHOICE`.

```txt
CHOICE
OPTION Purr to show you understand -> tutorial_purr
OPTION Paw at her hand for attention -> tutorial_paw
OPTION Hop down and inspect the room -> tutorial_inspect
ENDCHOICE
```

Each option jumps to a label after the `->`.

## Labels And Jumps

Use `LABEL` to mark a section of the story.

```txt
LABEL tutorial_purr
SAY Himari Good. A clever Tama.
GOTO first_night
```

Use `GOTO` to jump to another label.

```txt
GOTO first_night
```

Labels must be unique inside `assets/story.txt`.

## Variables

Variables store numbers for affection, route flags, or simple state.

```txt
SET himari_affection 0
ADD himari_affection 2
REDUCE himari_affection 1
ADD courage 1
```

`SET` gives a variable an exact value.

`ADD` increases a variable and shows two automatic narration lines:

```txt
*** himari_affection increased by 2 ***
Current himari_affection 2
```

`REDUCE` decreases a variable and also shows the current value:

```txt
*** himari_affection reduced by 1 ***
Current himari_affection 1
```

## Conditional Branches

Use `IF` to jump only when a variable matches a condition.

```txt
IF himari_affection >= 3 GOTO himari_night
IF reika_affection >= 2 GOTO reika_night
GOTO hallway_night
```

Supported operators:

```txt
==
!=
>=
<=
>
<
```

## Ending The File

Put `END` at the bottom.

```txt
END
```

The engine stops reading the story at `END`.

## Example Scene Pattern

```txt
LABEL himari_lounge
BG assets/backgrounds/lounge.png
SPRITE assets/sprites/emi_smile.png center
SAY Himari Back to me already?
SAY Narrator Himari leaves a sunny cushion open beside her.

CHOICE
OPTION Curl beside her notebook -> himari_notebook
OPTION Nudge her hand until she pets you -> himari_pets
ENDCHOICE

LABEL himari_notebook
ADD himari_affection 2
SAY Himari Helping me study? Then you are officially my assistant.
GOTO evening_check

LABEL himari_pets
ADD himari_affection 2
SAY Himari Needy today, are we?
GOTO evening_check
```

## Current Limits

- One active story file: `assets/story.txt`.
- Maximum choices per `CHOICE` block: 4.
- Only one sprite can be displayed at once.
- Text lines should be kept reasonably short.
- Labels must be unique.
