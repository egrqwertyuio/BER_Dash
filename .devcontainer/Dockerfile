# --------------------------------------------------------------------
  #SETUP
# --------------------------------------------------------------------
ARG VARIANT=bookworm
FROM mcr.microsoft.com/vscode/devcontainers/base:${VARIANT}

USER root

ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8
ENV DEBIAN_FRONTEND=noninteractive


RUN apt-get update -qq && \
 apt-get install -y -qq --no-install-recommends \
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
  python3-setuptools \
  udev \
  clang \
  hwdata \
  libusb-1.0-0 \ 
  usbutils \
  && apt-get clean \
  && apt-get purge \
  && rm -rf /var/lib/apt/lists/*




# Set up udev rules for PlatformIO
RUN curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules | sudo tee /etc/udev/rules.d/99-platformio-udev.rules
RUN service udev restart 
RUN usermod -a -G dialout vscode
RUN usermod -a -G plugdev vscode

# Install PlatformIO CLI
USER vscode
RUN curl -fsSL -o /tmp/get-platformio.py https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py && \
  python3 /tmp/get-platformio.py && \
  echo 'export PATH="$PATH:$HOME/.platformio/penv/bin"' | sudo tee -a /root/.bashrc 
