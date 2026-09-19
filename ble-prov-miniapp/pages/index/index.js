// pages/index/index.js - BLE 配网页面
Page({
  data: {
    ssid: '',
    password: '',
    devices: [],
    connected: false,
    deviceId: '',
    status: '未连接'
  },

  // 服务 UUID 和特征 UUID (与 ESP32-C3 固件一致)
  SERVICE_UUID: 'FFE0',
  CHAR_UUID: 'FFE1',

  onLoad() {
    this.checkBluetooth()
  },

  onUnload() {
    this.disconnect()
  },

  // 检查蓝牙是否可用
  checkBluetooth() {
    wx.openBluetoothAdapter({
      success: () => {
        console.log('蓝牙初始化成功')
        this.setData({ status: '蓝牙已就绪，点击扫描设备' })
      },
      fail: (err) => {
        console.error('蓝牙初始化失败', err)
        wx.showModal({
          title: '提示',
          content: '请开启手机蓝牙',
          showCancel: false
        })
      }
    })
  },

  // 扫描 BLE 设备
  scanDevices() {
    this.setData({ devices: [], status: '扫描中...' })
    wx.showLoading({ title: '扫描中...' })
    
    wx.startBluetoothDevicesDiscovery({
      allowDuplicatesKey: false,
      success: () => {
        wx.onBluetoothDeviceFound((res) => {
          const devices = res.devices.map(device => ({
            deviceId: device.deviceId,
            name: device.name || '未知设备',
            rssi: device.RSSI
          }))
          
          // 去重
          const allDevices = [...this.data.devices, ...devices].filter(
            (v, i, a) => a.findIndex(t => t.deviceId === v.deviceId) === i
          )
          
          this.setData({ devices: allDevices })
        })
        
        // 5秒后停止扫描
        setTimeout(() => {
          wx.stopBluetoothDevicesDiscovery()
          wx.hideLoading()
          this.setData({ status: `扫描完成，发现 ${this.data.devices.length} 个设备` })
        }, 5000)
      },
      fail: (err) => {
        wx.hideLoading()
        wx.showToast({ title: '扫描失败', icon: 'none' })
        this.setData({ status: '扫描失败' })
      }
    })
  },

  // 连接设备
  connectDevice(e) {
    const deviceId = e.currentTarget.dataset.id
    const deviceName = e.currentTarget.dataset.name
    
    wx.showLoading({ title: '连接中...' })
    
    wx.createBLEConnection({
      deviceId: deviceId,
      timeout: 10000,
      success: () => {
        wx.hideLoading()
        this.setData({ 
          connected: true, 
          deviceId: deviceId,
          status: '已连接: ' + deviceName
        })
        
        // 获取 MTU (最大传输单元)
        wx.setBLEMTU({
          deviceId: deviceId,
          mtu: 200
        })
        
        wx.showToast({ title: '连接成功', icon: 'success' })
      },
      fail: (err) => {
        wx.hideLoading()
        console.error('连接失败', err)
        wx.showToast({ title: '连接失败', icon: 'none' })
      }
    })
  },

  // 发送 WiFi 配置
  sendWifiConfig() {
    const { ssid, password, deviceId, connected } = this.data
    
    if (!connected) {
      wx.showToast({ title: '请先连接设备', icon: 'none' })
      return
    }
    
    if (!ssid) {
      wx.showToast({ title: '请输入WiFi名称', icon: 'none' })
      return
    }
    
    if (!password) {
      wx.showToast({ title: '请输入WiFi密码', icon: 'none' })
      return
    }
    
    // 格式: "SSID:PASSWORD"
    const data = `${ssid}:${password}`
    const buffer = this.stringToArrayBuffer(data)
    
    wx.showLoading({ title: '发送中...' })
    
    wx.writeBLECharacteristicValue({
      deviceId: deviceId,
      serviceId: this.SERVICE_UUID,
      characteristicId: this.CHAR_UUID,
      value: buffer,
      success: () => {
        wx.hideLoading()
        wx.showToast({ title: '发送成功', icon: 'success' })
        this.setData({ status: '配置已发送，设备正在连接WiFi...' })
      },
      fail: (err) => {
        wx.hideLoading()
        console.error('发送失败', err)
        wx.showToast({ title: '发送失败', icon: 'none' })
      }
    })
  },

  // 字符串转 ArrayBuffer
  stringToArrayBuffer(str) {
    const buffer = new ArrayBuffer(str.length)
    const dataView = new DataView(buffer)
    for (let i = 0; i < str.length; i++) {
      dataView.setUint8(i, str.charCodeAt(i))
    }
    return buffer
  },

  // 输入处理
  onSsidInput(e) {
    this.setData({ ssid: e.detail.value })
  },
  
  onPasswordInput(e) {
    this.setData({ password: e.detail.value })
  },

  // 断开连接
  disconnect() {
    if (this.data.deviceId) {
      wx.closeBLEConnection({
        deviceId: this.data.deviceId,
        complete: () => {
          this.setData({ 
            connected: false, 
            deviceId: '',
            status: '已断开' 
          })
        }
      })
    }
  }
})
