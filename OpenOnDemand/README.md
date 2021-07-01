# Installing Open OnDemand from github source.

We also install ruby, apache httpd, nodeJS, nginx, and slurm from source.
The scripts downloads and check all the source.  The package versions may
be configured (in the file config).

## Ports:
Ubuntu-mate 18.04.3
Debian 9
We expect that most Debian/Ubuntu derivatives will work.


## References

https://github.com/OSC/ondemand

https://osc.github.io/ood-documentation/master/installation/install-software.html

And urls listed in config.


## What is this

Same bash scripts that install an Open OnDemand service all from source.
All the scripts are pretty short, so looking at them will not hurt to
much.


## Usage

  *run% sudo ./root_apt_install*

  *edit config*

The file *config* is sourced by the *install_\** scripts.

  *run% ./install_ruby*

  *run% ./install_apachehttpd*

  *run% ./install_node*

  *run% ./install_nginx*

  *run% ./install_slurm*

Now you will have installed the softwares that are needed for Open
OnDemain.   You need to play sys-admin and fix your PATH and so on,
so that you can build and install ondemand.

  *run% ./install_ondemand*

If your really feeling like automating just edit *config* and run
*install_all*.  *install_all* runs all the scripts shown above, but it
adds steps that "play sys-admin" between some of the package installing,
so you'll need to edit it unless you are me.


## For purists

Instead of editing the source file *config* you can copy *config* to
*my_config* and edit that file.  Then running the programs will
automatically source *my_config* after sourcing *config*, adding your
custom configuation without you editing a source file for a repository.



## Why

Looks like the "Open OnDemand service" is targeted torward RedHat based
systems, but we are using Debian like systems for many reasons.  We are
developing other softwares that need to connect with the Open OnDemand
service, so we need to have good control over what the source code is,
so that this has a chance of being useful in the future.  A single use
installation is not our goal.  In order to keep all the service and even
Ruby compatable we needed the ablity to pick the versions of ruby, apache
httpd, nodeJS, nginx, and slurm.  Well maybe there was a way to make them,
these (Debian) APT packages, compatable with Open OnDemain, but we know
that APT upgrades would not as likely to brake our Open OnDemand service,
given that the listed packages would not change on "apt dist-upgrade".  So
until the Open OnDemand service distributs as a Debian package (that
doesn't suck), this is how we roll.
