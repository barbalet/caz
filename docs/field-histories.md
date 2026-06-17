# Early Field Histories

These histories are fictional in-universe notes for the Caz project. They are written in the tone of a quiet British rural documentary: patient, observational, fond of practical people, and attentive to weather, work, and small machines going about their business.

## The First Yard, Late April

At the edge of a lane in north Oxfordshire, where the fields fall away in careful green steps and the hedges have not yet decided whether spring is a promise or a rumour, the first Caz-powered cat droid was carried into a farmyard in a blue plastic crate.

It did not arrive triumphantly. It arrived with a checklist, two spare battery packs, and a note taped to its service bay reading: "Do not let it under the feed bins until gait 3 is fixed."

The droid was called Wicket by the farm children and Unit C-03 by the engineers. Caz itself knew nothing of either name. At `0x0100`, it knew only the beginning of its loop. Read the ears. Compare the sound. Read the eyes. Compare the motion. If the world is loud, retreat. If the world rustles, crouch. If the world is ordinary, walk.

The first morning was a morning of small negotiations. Wicket learned the scraped note of a yard broom as machine-pattern noise, which was wrong but understandable. It treated a bootlace as prey. It greeted a farmhand's whistle with a chirrup that had not been requested in the trial plan and was therefore written down very carefully.

By noon, the engineers had stopped watching the laptop and started watching the ears. When the ears went forward, the droid became legible. When they went flat, everyone stepped back. The farmer, who had been sceptical in the economical way of people who repair their own gates, gave the highest praise available on the day: "Well, it knows the tractor's not its friend."

## The Dairy Passage Trial

The second release was not in a picturesque meadow. It was in a dairy passage where everything useful had a dent in it.

Here the Caz droid met echoes. Sound bounced from block walls and returned wearing different clothes. A pail became a bell. A door latch became a tiny thunderstorm. The early `EAR_BEARING` values wandered, and Wicket, now joined by a second unit called Brindle, turned their heads in the wrong direction with great conviction.

The fix was not glamorous. The team widened the machine threshold, slowed the head yaw response, and added a habit of asking the eyes before committing to a pounce. It was the sort of change that makes a robot less impressive in a demonstration and far better company in a farm building.

The language helped. Because Caz was small, the farmer could follow the argument:

```asm
IN   A,(EAR_PATTERN)
CP   1
JP   Z,prey
```

"So that line is where it thinks it heard a mouse?"

Yes. That line.

After that, Caz was no longer an invisible intelligence. It was a visible set of decisions, each one small enough to be challenged by someone who knew the place better than the machine did.

## Rain On The Tin Roof

In June, the weather did the testing.

Rain on a tin roof is not one sound but a crowd of them. It arrives from above, from the yard, from the open door, from the old feed hopper, from a coat sleeve. To the first classifier it was prey, machine, human, and unknown in alternating mouthfuls.

The droids became busy and foolish. One crouched at a downpipe for eleven minutes. Another retreated from its own charging lead after a gust tapped it against the skirting board.

This was the day `weather` became a first-class ear pattern. The Caz program did not try to be clever. It simply allowed weather to be weather:

```asm
IN   A,(EAR_PATTERN)
CP   3
JP   Z,shelter
```

The result was oddly charming. In rain, the droids loafed under the covered passage, ears swivelling, tails curled, doing very little. For the first time they seemed less like test equipment and more like something that had read the room.

## The Parish Demonstration

The first public demonstration took place after a village hall talk about small robots in agriculture. There were folding chairs, urn tea, a plate of biscuits, and at the back of the room a cardboard pen with straw in it, as if the droids might prefer the familiar.

The Caz team expected questions about batteries and cost. They got questions about temperament.

Could it be made less nervous around tractors?

Could it recognise one person's voice?

Would it bother real cats?

Would it come when called, and if so, should it?

The last question stayed with the project. A useful rural robot is not merely an appliance with legs. It enters a landscape already full of habits, animals, routines, and old permissions. The Caz approach, with its small bytecode loops and visible thresholds, made temperament something that could be tuned in public.

The demonstration ended when Brindle detected a biscuit wrapper as prey and performed a short, deeply committed pounce into the front row. No harm was done. The wrapper was recovered. The line in the notebook reads: "Pounce threshold too theatrical indoors."

## Notes From The Hedgerow Release

The hedgerow release was the first trial where the droids were allowed to be briefly uninteresting. This was considered progress.

They walked the edge of a barley field. They paused at gaps in the hedge. They ignored most birds, most wind, and nearly all human conversation. When a real cat appeared on the wall, the droids did not pursue it. They lowered their tails and looked away, which had been implemented as a safety behaviour but was interpreted by everyone present as manners.

The important data came from ordinary moments:

- In broken sunlight, `EYE_LUMA` jumped but `EYE_MOTION` stayed low, so the droid narrowed its eyelids but did not chase shadows.
- Near the grain store, `EAR_PATTERN` became machine and the droid retreated without drama.
- Under the hedge, prey-like rustle and strong edge confidence combined into a crouch rather than an immediate pounce.

From the outside, this looked like caution. In the Caz trace, it was a dozen bytes of comparison and branch.

## What The Early Releases Taught

The earliest Caz cat droids were not persuasive because they were complex. They were persuasive because their simplicity could be watched.

A farmer could say, "It should not jump at that sound."

An engineer could answer, "Then we change that comparison."

A child could say, "Its ears are wrong."

And often, the child was right.

The project moved forward by keeping the operating system small enough that rural knowledge could meet machine behaviour in the middle. The droids did not need to pretend to be cats. They needed to be understandable cat-shaped machines, observant in the yard, cautious under rain, and just curious enough to make people lean closer.
