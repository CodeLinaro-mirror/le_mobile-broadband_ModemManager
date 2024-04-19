# SPDX-License-Identifier:  LGPL-2.0
# Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
ip addr add $1/$2 dev $3
ifconfig $3 up
ip route add default dev $3
