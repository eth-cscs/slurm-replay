# select image
FROM base/archlinux
MAINTAINER Maxime Martinasso <maxime.martinasso@cscs.ch>
ARG SLURM_VERSION=17.02.9
ARG REPLAY_USER=slurm

ENV SLURM_VERSION $SLURM_VERSION
ENV REPLAY_USER $REPLAY_USER

# install packages
# Note do not install sudo - sudo does not work within Shifter
RUN pacman -Sy --noconfirm autoconf automake gcc make mariadb wget patch python gtk2 pkgconf git fakeroot vim bc && \
               rm -rf /var/cache/pacman/pkg

# For debugging
# before to commit the container use the option docker run --cap-add sys_ptrace to be able to attach a debugger
#RUN pacman -Sy --noconfirm gdb valgrind strace && \
#               rm -rf /var/cache/pacman/pkg

# set timezone to CET
RUN  ln -sf /usr/share/zoneinfo/CET /etc/localtime

# create a user slurm and set mariadb to be user dependent (non-root)
RUN useradd -ms /bin/bash $REPLAY_USER && \
    mkdir -p /run/mysqld && \
    ln -s /home/$REPLAY_USER/run/mysqld/mysqld.lock /run/mysqld/mysqld.sock && \
    sed -i -e "s/socket[[:space:]]*=.*/socket=\/home\/$REPLAY_USER\/run\/mysqld\/mysqld.lock/g" /etc/mysql/my.cnf

# munge is in AUR
USER $REPLAY_USER
RUN cd /home/$REPLAY_USER && \
    git clone https://aur.archlinux.org/munge.git && \
    cd munge && \
    makepkg -s --noconfirm
USER root
# take opportunity to be root to copy slurm-replay and chown it
COPY . /home/$REPLAY_USER/slurm-replay
RUN pacman -U --noconfirm /home/$REPLAY_USER/munge/munge-*.pkg.tar.xz && \
    rm -Rf /home/$REPLAY_USER/munge && \
    chown -R $REPLAY_USER:$REPLAY_USER /home/$REPLAY_USER/slurm-replay

USER $REPLAY_USER

# install replay libraries - libwtime need to be built before slurm
RUN cd /home/$REPLAY_USER/slurm-replay/distime && \
    make

# get/patch/compile/install slurm (add dependency to libwtime)
RUN cd /home/$REPLAY_USER && \
    wget https://download.schedmd.com/slurm/slurm-$SLURM_VERSION.tar.bz2 && \
    tar jxf slurm-$SLURM_VERSION.tar.bz2 && \
    cd slurm-$SLURM_VERSION && \
    patch -p1 < ../slurm-replay/patch/slurm_shmemclock.patch && \
    patch -p1 < ../slurm-replay/patch/slurm_avoidstepmonitor.patch && \
    ./autogen.sh && \
    ./configure --prefix=/home/$REPLAY_USER/slurmR --enable-pam --enable-front-end --with-clock=/home/$REPLAY_USER/slurm-replay/distime --disable-debug && \
    make -j4 && make -j4 install && \
    mkdir /home/$REPLAY_USER/slurmR/etc && \
    mkdir /home/$REPLAY_USER/slurmR/log && \
    rm -Rf /home/$REPLAY_USER/slurm-$SLURM_VERSION.tar.bz2 && \
    rm -Rf /home/$REPLAY_USER/slurm-$SLURM_VERSION/ && \
    cd /home/$REPLAY_USER/slurm-replay && \
    ln -s /home/$REPLAY_USER/slurmR/log log && \
    ln -s /home/$REPLAY_USER/slurmR/etc etc

# install replay binaries, need to be done after installing slurm (depend on libslurm)
RUN cd /home/$REPLAY_USER/slurm-replay/submitter && \
    make

# create volume where traces and databases tables are located
# use "-v " when starting the container
RUN mkdir /home/$REPLAY_USER/data && \
    mkdir -p /home/$REPLAY_USER/var/lib && \
    mkdir -p /home/$REPLAY_USER/run/mysqld && \
    mkdir /home/$REPLAY_USER/tmp

#
# DOCKER command to run:
#     docker run --rm -it -v /Users/maximem/dev/docker/slurm-replay/data:/home/$REPLAY_USER/data --name slurm-replay slurm-replay
#
# SHIFTER command to run:
#    shifter --image mmxcscs/slurm-replay:maximem_slurm-17.02.9 --writable-volatile /home/maximem/slurmR/log --writable-volatile /home/maximem/slurmR/etc  --writable-volatile /home/maximem/var --writable-volatile /home/maximem/var/lib --writable-volatile /home/maximem/run --writable-volatile /home/maximem/run/mysqld --writable-volatile /home/maximem/tmp --writable-volatile /home/maximem/slurm-replay/submitter --volume /users/maximem/dev/data:/home/maximem/data /bin/bash
#

# for convenience
WORKDIR /home/$REPLAY_USER/slurm-replay
RUN echo "export PATH=\$PATH:/home/$REPLAY_USER/slurmR/bin:/home/$REPLAY_USER/slurmR/sbin:/home/$REPLAY_USER/slurm-replay/submitter" >> /home/$REPLAY_USER/.bashrc && \
    echo "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:/home/$REPLAY_USER/slurmR/lib:/home/$REPLAY_USER/slurm-replay/distime" >> /home/$REPLAY_USER/.bashrc && \
    echo "alias vi='vim'" >> /home/$REPLAY_USER/.bashrc

# invoke shell
CMD ["/bin/bash"]
