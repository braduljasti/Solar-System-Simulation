# Solar-System-Simulation
Solar System Simulation 🌌 A dynamic 3D simulation of the solar system built using OpenGL and C++.

Features:
Frame Rate Limitation: Smooth execution limited to 60 frames per second.

Comprehensive Celestial Objects:
Includes the Sun, all planets (including dwarf planets), moons, and two asteroid belts.
Each planet has accurate representations (e.g., Saturn with its rings).

Object Interactivity:
Hover over objects to display their name next to the cursor the screen.

Click on objects to display detailed information, including:
Name, mass, size, type, and a random fun fact (trivia).

Realistic Motion:
Planets move in correct orbits and speeds.
Planets also rotate on their axes.
Uses GLM library and transformations for accurate modeling.

Orbit Visibility:
Toggle planet orbits on/off using the keyboard.

Simulation Controls:
Adjust simulation speed using hotkeys.
Pause/resume the simulation as needed.

Additional libraries:
freetype.lib (version 2.10.0)

Controls:
Keys 1-5 are used to control the speed of simulation (1-0.5x speed; 2-1x speed; 3-2x speed; 4-5x speed; 5-10xspeed).
Mouse scroll wheel up/down and +/- are used for zoom in or zoom out.
Left clicking a celestial body shows info about it in the bottom right corner.
Keys WASD are used for moving around the system.
Hovering on a celestial body reveals the name of it.
Key O is used for disabling/enabling orbits of all rotating bodies in the system.
Key ESC is used for exiting the program.
key SPACE is used for pausing/resuming the simulation.
