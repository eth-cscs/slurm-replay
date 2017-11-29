# select image
FROM base/archlinux
MAINTAINER Maxime Martinasso <maxime.martinasso@cscs.ch>

# install packages
RUN pacman -Sy --noconfirm \
               autoconf automake gcc make mariadb wget patch python gtk2 pkgconf git fakeroot vim sudo bc \
               && rm -rf /var/cache/pacman/pkg

# set timezone to CET
RUN  ln -sf /usr/share/zoneinfo/CET /etc/localtime

# setup db
RUN groupadd -r mysql && \
    useradd -r -g mysql mysql && \
    mysql_install_db --user=mysql --basedir=/usr --datadir=/var/lib/mysql && \
    mkdir /run/mysqld && \
    chown mysql:mysql /run/mysqld

EXPOSE 3306

# create a user slurm with sudo access for using pacman and starting the mysql server
RUN useradd -ms /bin/bash slurm && \
    echo "slurm ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/slurm && \
    chmod 0440 /etc/sudoers.d/slurm
USER slurm
WORKDIR /home/slurm

# munge is in AUR
RUN git clone https://aur.archlinux.org/munge.git && \
    cd munge && \
    makepkg -s --noconfirm
RUN sudo pacman -U --noconfirm munge/munge-*.pkg.tar.xz

# install replay libraries
COPY --chown=slurm:slurm . /home/slurm/slurm-replay
RUN cd /home/slurm/slurm-replay/distime && \
    make

# get/patch/compile/install slurm
RUN cd /home/slurm && \
    wget https://download.schedmd.com/slurm/slurm-17.02.9.tar.bz2 && \
    tar jxf slurm-17.02.9.tar.bz2 && \
    cd slurm-17.02.9 && \
    patch -p1 < ../slurm-replay/patch/slurm_shmemclock.patch && \
    patch -p1 < ../slurm-replay/patch/slurm_avoidstepmonitor.patch && \
    ./autogen.sh && \
    ./configure --prefix=/home/slurm/slurmR --enable-pam --enable-front-end --with-clock=/home/slurm/slurm-replay/distime --disable-debug && \
    make -j4 && make -j4 install && \
    mkdir /home/slurm/slurmR/etc && \
    mkdir /home/slurm/slurmR/log

# default slurm port
EXPOSE 6819
# port setup in slurm.conf
EXPOSE 6817
EXPOSE 6818
EXPOSE 7000
EXPOSE 7321

# install replay binaries, need to be done after installing slurm
RUN cd /home/slurm/slurm-replay/submitter && \
    make

# configure slurm
RUN cd /home/slurm/slurm-replay && \
    ln -s ../slurmR/log log && \
    ln -s ../slurmR/etc etc && \
    cp conf/*.conf etc

# create volume where traces and databases tables are located
# use "-v " when starting the container
RUN mkdir /home/slurm/data

#
# DOCKER command to run:
# docker run -it -v <full path to your data i.e. traces and tables>:/home/slurm/data
# example:
#     docker run -it -v /Users/maximem/dev/docker/slurm-replay/data:/home/slurm/data --name slurm-replay slurm-replay
#

# for convenience
RUN echo "export PATH=\$PATH:/home/slurm/slurmR/bin:/home/slurm/slurmR/sbin:/home/slurm/slurm-replay/submitter" >> /home/slurm/.bashrc
WORKDIR /home/slurm/slurm-replay

# invoke shell
CMD ["/bin/bash"]
