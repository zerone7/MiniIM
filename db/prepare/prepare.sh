#!/bin/sh

mysql -u root -p < im_new_user.sql
mysql -u root -p< im_database.sql
mysql -u root -p < im_user.sql
mysql -u root -p< im_contact.sql
mysql -u root -p< im_offline_msg.sql
