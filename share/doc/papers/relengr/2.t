.\"	@(#)2.t	1.3	(Copyright 1989 M. K. McKusick)	89/02/23
.NH
System Development
.PP
The first phase of each Berkeley system is its development.
.SM CSRG
maintains a continuously evolving list of projects that are candidates
for integration into the system.
Some of these are prompted by emerging ideas from the research world,
such as the availability of a new technology, while other additions
are suggested by the commercial world, such as the introduction of
new standards like
.SM POSIX ,
and still other projects are emergency responses to situations like
the Internet Worm.
.PP
These projects are ordered based on the perceived benefit of the
project as opposed to its difficulty, and some number of them are
selected for inclusion in each new release.
Usually there is a prototype available from a group outside of
.SM CSRG .
If possible, this prototype is obtained to use as a starting base
for integration into the
.SM BSD
system.
Only if no prototype is available is the project architected in-house.
.PP
Unlike other development groups, the staff of
.SM CSRG
specializes by projects rather than by particular parts
of the system;
a staff person will be responsible for all aspects of a project.
This responsibility starts at the associated kernel device drivers;
it proceeds up through the rest of the kernel,
through the C library and system utility programs,
ending at the user application layer.
This staff person is also responsible for related documentation,
including manual pages.
Many projects proceed in parallel,
interacting with other projects as their paths cross.
.PP
Much of the development of
.SM BSD
is done by personnel that are located at other institutions.
Many of these people not only have interim copies of the release
running on their own machines,
but also have user accounts on the main development
machine at Berkeley.
Such users are commonly found logged in at Berkeley over the
Internet, or sometimes via telephone dialup, from places as far away
as Massachusetts or Maryland, as well as from closer places, such as
Stanford.
For the \*(b3 release,
certain users had permission to modify the master copy of the
system source directly.
People given access to the master sources
are carefully screened beforehand,
but are not closely supervised.
Their work is checked at the end of the beta-test period by
.SM CSRG
personnel who back out inappropriate changes.
Several facilities, including the
Fortran and C compilers,
as well as important system programs, for example,
.PN telnet
and
.PN ftp ,
include significant contributions from people who did not work
directly for
.SM CSRG .
One important exception to this approach is that changes to the kernel
are made only by
.SM CSRG
personnel, although the changes are often suggested by the larger community.
.PP
All source code, documentation, and auxiliary files are kept
under a source code control system.
During development,
this control system is critical for notifying people
when they are colliding with other ongoing projects.
Even more important, however,
is the audit trail maintained by the control system which
is critical to the release engineering phase of the project
described in the next section.
.PP
The development phase continues until
.SM CSRG
decides that it is appropriate to make a release.
The decision to halt development and transition to release mode
is driven by several factors.
The most important is that sufficient projects have been completed
to make the system significantly superior to the previously released
version of the system.
For example,
\*(b3 was released primarily because of the need for
the improved networking capabilities and the markedly
improved system performance.
Of secondary importance is the issue of timing.
If the releases are too infrequent, then
.SM CSRG
will be inundated with requests for interim releases.
Conversely,
if systems are released too frequently,
the integration cost for many vendors will be too high,
causing them to ignore the releases.
Finally,
the process of release engineering is long and tedious.
Frequent releases slows the rate of development and
causes undue tedium to the staff.
