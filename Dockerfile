# select image
FROM base/archlinux
MAINTAINER Maxime Martinasso <maxime.martinasso@cscs.ch>
ARG SLURM_VERSION=17.02.9
ARG REPLAY_USER=slurm

ENV SLURM_VERSION $SLURM_VERSION
ENV REPLAY_USER $REPLAY_USER

# install packages
RUN pacman -Sy --noconfirm \
               autoconf automake gcc make mariadb wget patch python gtk2 pkgconf git fakeroot vim sudo bc \
               && rm -rf /var/cache/pacman/pkg

# set timezone to CET
RUN  ln -sf /usr/share/zoneinfo/CET /etc/localtime

# create a user slurm with sudo access for using pacman
RUN useradd -ms /bin/bash $REPLAY_USER && \
    echo "$REPLAY_USER ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/$REPLAY_USER && \
    chmod 0440 /etc/sudoers.d/$REPLAY_USER && \
    mysql_install_db --user=$REPLAY_USER --basedir=/usr --datadir=/home/$REPLAY_USER/var/lib && \
    mkdir /run/mysqld && \
    chown -R $REPLAY_USER:$REPLAY_USER /run/mysqld
EXPOSE 3306
USER $REPLAY_USER

# munge is in AUR
RUN cd /home/$REPLAY_USER && \
    git clone https://aur.archlinux.org/munge.git && \
    cd munge && \
    makepkg -s --noconfirm
RUN sudo pacman -U --noconfirm /home/$REPLAY_USER/munge/munge-*.pkg.tar.xz && \
    rm -Rf /home/$REPLAY_USER/munge

# install replay libraries
COPY . /home/$REPLAY_USER/slurm-replay
RUN sudo chown -R $REPLAY_USER:$REPLAY_USER /home/$REPLAY_USER/slurm-replay && \
    cd /home/$REPLAY_USER/slurm-replay/distime && \
    make

# get/patch/compile/install slurm
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


# default slurm port
EXPOSE 6819
# port setup in slurm.conf
EXPOSE 6817
EXPOSE 6818
EXPOSE 7000
EXPOSE 7321

# install replay binaries, need to be done after installing slurm
RUN cd /home/$REPLAY_USER/slurm-replay/submitter && \
    make

# create volume where traces and databases tables are located
# use "-v " when starting the container
RUN mkdir /home/$REPLAY_USER/data && \
    mkdir -p /home/$REPLAY_USER/var/lib && \
    mkdir /home/$REPLAY_USER/tmp

#
# DOCKER command to run:
# docker run -it -v <full path to your data i.e. traces and tables>:/home/$REPLAY_USER/data
# example:
#     docker run --rm -it -v /Users/maximem/dev/docker/slurm-replay/data:/home/$REPLAY_USER/data --name slurm-replay slurm-replay
#

# for convenience
WORKDIR /home/$REPLAY_USER/slurm-replay
RUN echo "export PATH=\$PATH:/home/$REPLAY_USER/slurmR/bin:/home/$REPLAY_USER/slurmR/sbin:/home/$REPLAY_USER/slurm-replay/submitter" >> /home/$REPLAY_USER/.bashrc && \
    echo "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:/home/$REPLAY_USER/slurmR/lib:/home/$REPLAY_USER/slurm-replay/distime" >> /home/$REPLAY_USER/.bashrc && \
    echo "alias vi='vim'" >> /home/$REPLAY_USER/.bashrc


# invoke shell
CMD ["/bin/bash"]
