# select image
FROM base/archlinux
MAINTAINER Maxime Martinasso <maxime.martinasso@cscs.ch>
ARG SLURM_VERSION=17.02.9
ARG REPLAY_USER=slurm

ENV SLURM_VERSION $SLURM_VERSION
ENV REPLAY_USER $REPLAY_USER

# install packages
# Note do not install sudo - sudo does not work within Shifter
RUN pacman -Sy --noconfirm autoconf automake git gawk gcc mpfr make mariadb wget patch python gtk2 pkgconf fakeroot vim bc groff gdb valgrind strace && \
               rm -rf /var/cache/pacman/pkg

# set timezone to CET
RUN  ln -sf /usr/share/zoneinfo/CET /etc/localtime

# create a user slurm and set mariadb to be user dependent (non-root)
# do not /home in case it cannot be mounted by the container technology
RUN useradd -ms /bin/bash -d /$REPLAY_USER $REPLAY_USER && \
    mkdir -p /run/mysqld && \
    ln -s /$REPLAY_USER/run/mysqld/mysqld.lock /run/mysqld/mysqld.sock && \
    sed -i -e "s/socket[[:space:]]*=.*/socket=\/$REPLAY_USER\/run\/mysqld\/mysqld.lock/g" /etc/mysql/my.cnf && \
    sed -i -e "s/#innodb_buffer_pool_size[[:space:]]*=.*/innodb_buffer_pool_size=1024M/g" /etc/mysql/my.cnf && \
    sed -i -e "s/#innodb_log_file_size[[:space:]]*=.*/innodb_log_file_size=64M/g" /etc/mysql/my.cnf && \
    sed -i -e "s/#innodb_lock_wait_timeout[[:space:]]*=.*/innodb_lock_wait_timeout=900/g" /etc/mysql/my.cnf

USER $REPLAY_USER
COPY . /$REPLAY_USER/slurm-replay
COPY slurm-$SLURM_VERSION.tar.bz2 /$REPLAY_USER

USER root
RUN chown -R $REPLAY_USER:$REPLAY_USER /$REPLAY_USER/slurm-replay
RUN chown -R $REPLAY_USER:$REPLAY_USER /$REPLAY_USER/slurm-$SLURM_VERSION.tar.bz2

USER $REPLAY_USER
# install replay libraries - libwtime need to be built before slurm
RUN cd /$REPLAY_USER/slurm-replay/distime && \
    make

# get/patch/compile/install slurm (add dependency to libwtime)
    #wget https://download.schedmd.com/slurm/slurm-$SLURM_VERSION.tar.bz2 && \
RUN cd /$REPLAY_USER && \
    tar jxf slurm-$SLURM_VERSION.tar.bz2 && \
    cd slurm-$SLURM_VERSION && \
    patch -p1 < ../slurm-replay/patch/slurm_shmemclock.patch && \
    patch -p1 < ../slurm-replay/patch/slurm_avoidstepmonitor.patch && \
    patch -p1 < ../slurm-replay/patch/slurm_explicitpriority.patch && \
    ./autogen.sh && \
    ./configure --prefix=/$REPLAY_USER/slurmR --enable-pam --enable-front-end --with-clock=/$REPLAY_USER/slurm-replay/distime --disable-debug --without-munge CFLAGS="-g -O3 -D NDEBUG=1" && \
    make -j4 && make -j4 install && \
    mkdir /$REPLAY_USER/slurmR/etc && \
    mkdir /$REPLAY_USER/slurmR/log && \
    rm -Rf /$REPLAY_USER/slurm-$SLURM_VERSION.tar.bz2 && \
    cd /$REPLAY_USER/slurm-replay && \
    ln -s /$REPLAY_USER/slurmR/log log && \
    ln -s /$REPLAY_USER/slurmR/etc etc

#    rm -Rf /$REPLAY_USER/slurm-$SLURM_VERSION/ && \
# install replay binaries, need to be done after installing slurm (depend on libslurm)
RUN cd /$REPLAY_USER/slurm-replay/submitter && \
    make

# create volume where traces and databases tables are located
# use "-v " when starting the container
RUN mkdir /$REPLAY_USER/data && \
    mkdir -p /$REPLAY_USER/var/lib && \
    mkdir -p /$REPLAY_USER/run/mysqld && \
    mkdir /$REPLAY_USER/tmp

#
# DOCKER build command:
#     docker build -t mmxcscs/slurm-replay:maximem_slurm-17.02.9 --build-arg REPLAY_USER=maximem .
#
# DOCKER command to run:
#     docker run --rm -it -v /Users/maximem/dev/docker/slurm-replay/data:/maximem/data mmxcscs/slurm-replay:maximem_slurm-17.02.9
# NOTE FOR DEBUGGING:
#     before to commit the container use the option docker run --cap-add sys_ptrace to be able to attach a debugger
#
#
# SHIFTER command to run:
#    shifter --image mmxcscs/slurm-replay:maximem_slurm-17.02.9 --writable-volatile /maximem/slurmR/log --writable-volatile /maximem/slurmR/etc  --writable-volatile /maximem/var --writable-volatile /maximem/var/lib --writable-volatile /maximem/run --writable-volatile /maximem/run/mysqld --writable-volatile /maximem/tmp --writable-volatile /maximem/slurm-replay/submitter --volume /users/maximem/dev/data:/maximem/data /bin/bash
#

# Install monthly report tools from CSCS
COPY monthlyrpts /$REPLAY_USER/slurm-replay
RUN cd /$REPLAY_USER/slurm-replay/monthlyrpts/2.0.0/src && \
    make && make install && \
    echo "export PATH=\$PATH:/$REPLAY_USER/slurm-replay/monthlyrpts/2.0.0/src" >> /$REPLAY_USER/.bashrc && \
    echo "export MONTHLYRPTS_ROOT=/maximem/slurm-replay/monthlyrpts/2.0.0" >> /$REPLAY_USER/.bashrc && \
    echo "export MONTHLYRPTS_RPTS=/maximem/slurm-replay/monthlyrpts/2.0.0/reports" >> /$REPLAY_USER/.bashrc && \
    echo "export MONTHLYRPTS_PLOTS=/maximem/slurm-replay/monthlyrpts/2.0.0/plots" >> /$REPLAY_USER/.bashrc


# for convenience
WORKDIR /$REPLAY_USER/slurm-replay
RUN echo "export PATH=\$PATH:/$REPLAY_USER/slurmR/bin:/$REPLAY_USER/slurmR/sbin:/$REPLAY_USER/slurm-replay/submitter:/$REPLAY_USER/slurm-replay/monthlyrpts/2.0.0/bin" >> /$REPLAY_USER/.bashrc && \
    echo "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:/$REPLAY_USER/slurmR/lib:/$REPLAY_USER/slurm-replay/distime" >> /$REPLAY_USER/.bashrc && \
    echo "alias vi='vim'" >> /$REPLAY_USER/.bashrc

# invoke shell
CMD ["/bin/bash"]
