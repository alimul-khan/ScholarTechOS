#!/bin/sh
# Set up one prayer screen from this Mac, after flashing it and connecting it to the network:
# gives it the read-only key it uses to install new app releases, optionally sets its time zone,
# then checks for an update right away.
#
#     scripts/provision.sh <screen address> [time zone]
#     scripts/provision.sh 10.0.0.59
#     scripts/provision.sh scholartech.local America/Toronto
#
# Needs: SSH access to the screen as root (your key in authorized_keys on the boot partition) and
# the deploy key at ~/.ssh/scholartech_prayer_deploy (or set PRAYER_DEPLOY_KEY). The key only reads
# display_template; it is copied to the screen, never into the OS image or a repo.
set -eu
HOST=${1:?usage: scripts/provision.sh <screen address> [time zone]}
ZONE=${2:-}
KEY=${PRAYER_DEPLOY_KEY:-$HOME/.ssh/scholartech_prayer_deploy}

[ -f "$KEY" ] || { echo "No deploy key at $KEY"; exit 1; }
case "$ZONE" in *[!A-Za-z0-9_/+-]*) echo "Not a time zone: $ZONE"; exit 1 ;; esac

ssh_screen() { ssh -o ConnectTimeout=10 -o StrictHostKeyChecking=accept-new "root@$HOST" "$@"; }

ssh_screen 'test -x /opt/scholartech/prayer-update' ||
    { echo "$HOST is not running the prayer version of ScholarTechOS"; exit 1; }

ssh_screen 'umask 077 && mkdir -p /etc/scholartech && cat > /etc/scholartech/prayer-deploy-key' < "$KEY"
echo "Deploy key installed on $HOST."

if [ -n "$ZONE" ]; then
    ssh_screen "test -e /usr/share/zoneinfo/$ZONE && ln -sf ../usr/share/zoneinfo/$ZONE /etc/localtime" ||
        { echo "Unknown time zone: $ZONE"; exit 1; }
    echo "Time zone: $ZONE"
fi

ssh_screen '/opt/scholartech/prayer-update; /opt/scholartech/prayer-update --status' || true
