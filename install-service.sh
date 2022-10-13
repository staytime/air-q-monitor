

pip3 install -r sensors-host/requirements.txt

sudo cp -r sensors-host /opt/sensors-host
sudo cp sensors-host.service /etc/systemd/system/sensors-host.service
sudo systemctl daemon-reload
sudo systemctl enable sensors-host.service
sudo systemctl restart sensors-host.service
