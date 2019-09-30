FROM debian:stable
MAINTAINER Maxime Martinasso <maxime.martinasso@cscs.ch>
ARG SLURM_VERSION=18.08.8
ARG REPLAY_USER=replayuser

ENV SLURM_VERSION $SLURM_VERSION
ENV REPLAY_USER $REPLAY_USER

# install packages
# Note do not install sudo - sudo does not work within Shifter
RUN apt-get update && apt-get --assume-yes install autoconf automake git gawk gcc libmpfr6 make wget patch python pkgconf fakeroot vim bc groff gdb valgrind strace psmisc lsof net-tools libtool gtk+2.0 mariadb-client mariadb-server libmariadb-dev libmariadbclient-dev

# set timezone to CET
RUN  ln -sf /usr/share/zoneinfo/CET /etc/localtime

# create a user slurm and set mariadb to be user dependent (non-root)
# do not use /home in case it cannot be mounted by the container technology
RUN useradd -ms /bin/bash -d /$REPLAY_USER $REPLAY_USER && \
    mkdir -p /var/run/mysqld && \
    mkdir -p /$REPLAY_USER/run/mysqld && \
    mkdir -p /$REPLAY_USER/var/lib/mysql && \
    mkdir -p /$REPLAY_USER/var/log/mysql && \
    mkdir -p /$REPLAY_USER/data && \
    ln -s /$REPLAY_USER/run/mysqld/mysqld.lock /var/run/mysqld/mysqld.sock && \
    sed -i -e "s/user[[:space:]]*=.*/user=$REPLAY_USER/g" /etc/mysql/mariadb.conf.d/50-server.cnf && \
    sed -i -e "s/socket[[:space:]]*=.*/socket=\/$REPLAY_USER\/run\/mysqld\/mysqld.lock/g" /etc/mysql/mariadb.conf.d/50-server.cnf && \
    sed -i -e "s/pid-file[[:space:]]*=.*/pid-file=\/$REPLAY_USER\/run\/mysqld\/mysqld.pid/g" /etc/mysql/mariadb.conf.d/50-server.cnf && \
    sed -i -e "s/datadir[[:space:]]*=.*/datadir=\/$REPLAY_USER\/var\/lib\/mysql/g" /etc/mysql/mariadb.conf.d/50-server.cnf && \
    sed -i -e "s/log_error[[:space:]]*=.*/log_error=\/$REPLAY_USER\/var\/log\/mysql\/error.log/g" /etc/mysql/mariadb.conf.d/50-server.cnf && \
    sed -i -e "s/socket[[:space:]]*=.*/socket=\/$REPLAY_USER\/run\/mysqld\/mysqld.lock/g" /etc/mysql/mariadb.conf.d/50-mysqld_safe.cnf && \
    sed -i -e "s/socket[[:space:]]*=.*/socket=\/$REPLAY_USER\/run\/mysqld\/mysqld.lock/g" /etc/mysql/mariadb.conf.d/50-client.cnf && \
    sed -i -e "/^\[client-server\]/a [mysqld]" /etc/mysql/mariadb.cnf && \
    sed -i -e "/^\[mysqld\]/a innodb_buffer_pool_size=1024M" /etc/mysql/mariadb.cnf && \
    sed -i -e "/^\[mysqld\]/a innodb_log_file_size=64M" /etc/mysql/mariadb.cnf && \
    sed -i -e "/^\[mysqld\]/a innodb_lock_wait_timeout=900" /etc/mysql/mariadb.cnf

USER $REPLAY_USER
COPY . /$REPLAY_USER/slurm-replay
COPY slurm-$SLURM_VERSION.tar.bz2 /$REPLAY_USER

USER root
RUN chown -R $REPLAY_USER:$REPLAY_USER /$REPLAY_USER

USER $REPLAY_USER
# install replay libraries - libwtime need to be built before slurm
RUN cd /$REPLAY_USER/slurm-replay/distime && \
    make

# get/patch/compile/install slurm (add dependency to libwtime)
#wget https://download.schedmd.com/slurm/slurm-$SLURM_VERSION.tar.bz2 && \
RUN cd /$REPLAY_USER && \
    tar jxf slurm-$SLURM_VERSION.tar.bz2 && \
    cd slurm-$SLURM_VERSION && \
    patch -p1 < ../slurm-replay/patch/slurm_cryptonone.patch && \
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

# install replay binaries, need to be done after installing slurm (it depends on libslurm)
RUN cd /$REPLAY_USER/slurm-replay/submitter && \
    make

#
# DOCKER build command:
#     docker build -t mmxcscs/slurm-replay:replayuser_slurm-18.08.8 --build-arg REPLAY_USER=replayuser .
#
# DOCKER command to run:
#     docker run --cap-add sys_ptrace --rm -it -v /Users/replayuser/dev/docker/slurm-replay/data:/replayuser/data --volume /Users/replayuser/dev/docker/slurm-replay/data/new_passwd:/etc/passwd --volume /Users/replayuser/dev/docker/slurm-replay/data/new_group:/etc/group -h localhost mmxcscs/slurm-replay:replayuser_slurm-18.08.8
# NOTE FOR DEBUGGING:
#     before to commit the container use the option docker run --cap-add sys_ptrace to be able to attach a debugger
#
#
# SHIFTER command to run:
# shifter run --writable-volatile=/replayuser/slurmR/log --writable-volatile=/replayuser/slurmR/etc  --writable-volatile=/replayuser/var --writable-volatile=/replayuser/var/lib --writable-volatile=/replayuser/run --writable-volatile=/replayuser/run/mysqld --writable-volatile=/replayuser/tmp --writable-volatile=/replayuser/slurm-replay/submitter --mount=source=/users/replayuser/dev/data,destination=/replayuser/data,type=bind  mmxcscs/slurm-replay:replayuser_slurm-18.08.8 /bin/bash
#

# for convenience
WORKDIR /$REPLAY_USER/slurm-replay
RUN echo "export PATH=\$PATH:/$REPLAY_USER/slurmR/bin:/$REPLAY_USER/slurmR/sbin:/$REPLAY_USER/slurm-replay/submitter:/$REPLAY_USER/slurm-replay/monthlyrpts/2.0.0/bin" >> /$REPLAY_USER/.bashrc && \
    echo "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:/$REPLAY_USER/slurmR/lib:/$REPLAY_USER/slurm-replay/distime" >> /$REPLAY_USER/.bashrc && \
    echo "alias vi='vim'" >> /$REPLAY_USER/.bashrc

# invoke shell
CMD ["/bin/bash"]
