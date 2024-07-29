#!/bin/bash

# Deployer for Travis-CI
# Default Variables
#
# Here are all of the user-set variables used by Deployer.
# See the "Cross-platform deployment" page on SRB2 Wiki for documentation.

# Core Parameters
: ${DPL_ENABLED}                # Enable Deployer behavior; must be set for any deployment activity
: ${DPL_TAG_ENABLED}            # Trigger Deployer for all tag releases
: ${DPL_JOB_ENABLE_ALL}         # Enable all jobs for deployment
: ${DPL_TERMINATE_TESTS}        # Terminate all build test jobs (used in .travis.yml)
: ${DPL_TRIGGER}                # Use a [word] in the commit message to trigger Deployer
: ${DPL_JOBNAMES}               # Trigger Deployer by job name
: ${DPL_OSNAMES}                # Trigger Deployer by OS name (osx,linux)
: ${DPL_BRANCHES}               # Trigger Deployer by git branch name

# Job Parameters
: ${_DPL_JOB_ENABLED}           # Enable Deployer for this specific job. DPL_ENABLED must be set too.
: ${_DPL_JOB_NAME}              # Identifier for the job, used for logging and trigger word matching
: ${_DPL_FTP_TARGET}            # Deploy to FTP
: ${_DPL_DPUT_TARGET}           # Deploy to DPUT
: ${_DPL_PACKAGE_SOURCE}        # Build packages into a Source distribution. Linux only.
: ${_DPL_PACKAGE_BINARY}        # Build packages into a Binary distribution.
: ${_DPL_PACKAGE_MAIN:=1}       # Build main installation package. Linux only; OS X assumes this.
: ${_DPL_PACKAGE_ASSET}         # Build asset installation package. Linux only.

# Asset File Parameters
: ${ASSET_ARCHIVE_PATH:=https://github.com/mazmazz/Kart-Public/releases/download/kart_assets/srb2kart-v102-assets.7z}
: ${ASSET_ARCHIVE_OPTIONAL_PATH:=https://github.com/mazmazz/Kart-Public/releases/download/kart_assets/srb2kart-v102-optional-assets.7z}
: ${ASSET_FILES_HASHED:=main.pk3 chars.pk3 gfx.pk3 maps.pk3 textures.pk3 patch.pk3}
: ${ASSET_FILES_DOCS:=README.txt HISTORY.txt LICENSE.txt LICENSE-3RD-PARTY.txt README-SDL.txt}
: ${ASSET_FILES_OPTIONAL_GET:=0}

# FTP Parameters
: ${DPL_FTP_PROTOCOL}
: ${DPL_FTP_USER}
: ${DPL_FTP_PASS}
: ${DPL_FTP_HOSTNAME}
: ${DPL_FTP_PORT}
: ${DPL_FTP_PATH}

# DPUT Parameters
: ${DPL_DPUT_DOMAIN:=ppa.launchpad.net}
: ${DPL_DPUT_METHOD:=sftp}
: ${DPL_DPUT_INCOMING}
: ${DPL_DPUT_LOGIN:=anonymous}
: ${DPL_SSH_KEY_PRIVATE}        # Base64-encoded private key file. Used to sign repository uploads
: ${DPL_SSH_KEY_PASSPHRASE}     # Decodes the private key file.

# Package Parameters
: ${PACKAGE_NAME:=srb2kart}
: ${PACKAGE_VERSION:=2.0}
: ${PACKAGE_SUBVERSION}         # Highly recommended to set this to reflect the distro series target (e.g., ~18.04bionic)
: ${PACKAGE_REVISION}           # Defaults to UTC timestamp
: ${PACKAGE_INSTALL_PATH:=/usr/games/SRB2Kart}
: ${PACKAGE_LINK_PATH:=/usr/games}
: ${PACKAGE_DISTRO:=trusty}
: ${PACKAGE_URGENCY:=high}
: ${PACKAGE_NAME_EMAIL:=Kart Krew <kartkrewdev@gmail.com>}
: ${PACKAGE_GROUP_NAME_EMAIL:=Kart Krew <kartkrewdev@gmail.com>}
: ${PACKAGE_WEBSITE:=<https://mb.srb2.org/showthread.php?p=802695>}

: ${PACKAGE_ASSET_MINVERSION:=1.2} # Number this the version BEFORE the actual required version, because we do a > check
: ${PACKAGE_ASSET_MAXVERSION:=2.1}  # Number this the version AFTER the actual required version, because we do a < check

: ${PROGRAM_NAME:=Sonic Robo Blast 2 Kart}
: ${PROGRAM_VENDOR:=Kart Krew}
: ${PROGRAM_VERSION:=2.0}
: ${PROGRAM_DESCRIPTION:=Go go-karting with Dr. Eggman!}
: ${PROGRAM_FILENAME:=srb2kart}

: ${DPL_PGP_KEY_PRIVATE}        # Base64-encoded private key file. Used to sign Debian packages
: ${DPL_PGP_KEY_PASSPHRASE}     # Decodes the private key file.

# Export Asset and Package Parameters for envsubst templating

export ASSET_ARCHIVE_PATH="${ASSET_ARCHIVE_PATH}"
export ASSET_ARCHIVE_OPTIONAL_PATH="${ASSET_ARCHIVE_OPTIONAL_PATH}"
export ASSET_FILES_HASHED="${ASSET_FILES_HASHED}"
export ASSET_FILES_DOCS="${ASSET_FILES_DOCS}"
export ASSET_FILES_OPTIONAL_GET="${ASSET_FILES_OPTIONAL_GET}"

export PACKAGE_NAME="${PACKAGE_NAME}"
export PACKAGE_VERSION="${PACKAGE_VERSION}"
export PACKAGE_SUBVERSION="${PACKAGE_SUBVERSION}" # in case we have this
export PACKAGE_REVISION="${PACKAGE_REVISION}"
export PACKAGE_ASSET_MINVERSION="${PACKAGE_ASSET_MINVERSION}"
export PACKAGE_ASSET_MAXVERSION="${PACKAGE_ASSET_MAXVERSION}"
export PACKAGE_INSTALL_PATH="${PACKAGE_INSTALL_PATH}"
export PACKAGE_LINK_PATH="${PACKAGE_LINK_PATH}"
export PACKAGE_DISTRO="${PACKAGE_DISTRO}"
export PACKAGE_URGENCY="${PACKAGE_URGENCY}"
export PACKAGE_NAME_EMAIL="${PACKAGE_NAME_EMAIL}"
export PACKAGE_GROUP_NAME_EMAIL="${PACKAGE_GROUP_NAME_EMAIL}"
export PACKAGE_WEBSITE="${PACKAGE_WEBSITE}"

export PROGRAM_NAME="${PROGRAM_NAME}"
export PROGRAM_VERSION="${PROGRAM_VERSION}"
export PROGRAM_DESCRIPTION="${PROGRAM_DESCRIPTION}"
export PROGRAM_FILENAME="${PROGRAM_FILENAME}"

# This file is called in debian_template.sh, so mark our completion so we don't run it again
__DEBIAN_PARAMETERS_INITIALIZED=1
