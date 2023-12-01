# fsgrid - a lightweight, static, cartesian grid for field solvers

FsGrid is trying to be an uncomplicated, static (non-loadbalanced) cartesian grid for
use in field solvers of kinetic plasma simulations. Independent of the actual spatial
grid structure that the rest of the simulation is using, FsGrid has been designed to
quickly copy data in, solve fields with the light datastructures it provides, and
copy them out again, to continue computations on other (potentially more heavy-weight)
parts of the code.
