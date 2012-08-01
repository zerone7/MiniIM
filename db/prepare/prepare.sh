#!/bin/sh

mysql -u root < im_new_user.sql
mysql -u root < im_database.sql
mysql -u root < im_user.sql
mysql -u root < im_contact.sql
mysql -u root < im_offline_msg.sql
