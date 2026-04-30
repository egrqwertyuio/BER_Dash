# --------------------------------------------------------------------
  #SETUP
# --------------------------------------------------------------------
FROM debian:trixie-slim
USER root

RUN apt-get update -qq && \
 apt-get install -y -qq --no-install-recommends software-properties-common \
  apt-utils \
  sudo \
  curl \
  git \
  wget \
  build-essential \
  ca-certificates \
  vim \
  python3 \
  python3-dev \
  python3-pip \
  python3-venv \
  cmake \
  gcc \
  ninja-build \
  make \
  libffi-dev \
  libncurses-dev \
  libssl-dev \
  libusb-1.0-0 \
  ccache \
  && apt-get clean && rm -rf /var/lib/apt/lists/*


