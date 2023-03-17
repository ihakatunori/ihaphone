1 How to Setup

(1) Trisquel,Debian,ubuntu

   sudo apt install -y gcc make libgtk-3-dev libasound2-dev libjpeg-dev libssl-dev libgmp-dev ffmpeg

(2)Fedra

   sudo yum install -y gcc make gtk3-devel libjpeg-devel gmp-devel openssl-devel alsa-lib-devel

   sudo dnf install \
       https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm \
       https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm

   sudo dnf install ffmpeg

(3)CentOS 7

   su root

   yum install -y gcc make gtk3-devel libjpeg-devel gmp-devel openssl-devel alsa-lib-devel

   yum -y install epel-release
   rpm --import http://li.nux.ro/download/nux/RPM-GPG-KEY-nux.ro
   rpm -Uvh http://li.nux.ro/download/nux/dextop/el7/x86_64/nux-dextop-release-0-1.el7.nux.noarch.rpm
   yum -y install ffmpeg ffmpeg-devel

(4)Red Hat Enterprise Linux

   su root

   yum install -y gcc make gtk3-devel libjpeg-devel gmp-devel openssl-devel alsa-lib-devel

   install ffmpeg
     dnf -y install https://dl.fedoraproject.org/pub/epel/epel-release-latest...
     dnf install https://download1.rpmfusion.org/free/el/rpmfusion-free-re...
     dnf repolist --all | grep codeready-builder
     subscription-manager repos --enable "codeready-builder-for-rhel-8-$(uname -m)-rpms"
     dnf repolist
     dnf info ffmpeg
     dnf -y install ffmpeg

2 How to Build

 make all

3 How to Run

./ihaphone

4 How to generate new key

./ihakeygen THRESHOLD hashsize bitsno multi filename

      THRESHOLD : 100〜65535
      hashsize  : 0 or 32 or 64
      bitsno    : 1 or 2 or 4 or 8
      multi     : 2〜5
      filename  : key file name

        you must change parameter of init_cipher.

5 How to use address book

Edit ihaphone-address.txt with gedit.

