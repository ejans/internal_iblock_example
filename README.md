Internal iblock example
=======================

copy of the [random block] with internal iblock to store the configuration data needed in different states.

Layout
------

![][Internal_IBlock]

An internal fifo _i-block_ saves all the data specified as a configuration (min and max values of random number) and is saved in the start state.
The random number is checked according to this configuration at each step and the configuration is returned to the fifo.

License
-------

See COPYING, The license is GPLv2 with a linking exception.

Acknowledgement
---------------

This work was supported by the European FP7 project SHERPA (FP7-ICT-600958).

[random block]: https://github.com/kmarkus/microblx/tree/dev/std_blocks/random
[Internal_IBlock]: figs/Internal_IBlock.png?raw=true
