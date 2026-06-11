'use strict';

const express = require('express');
const config = require('../config');
const recordStore = require('../services/recordStore');
const aliyunHistory = require('../services/aliyunHistory');

const router = express.Router();

/** GET /records — App 拉取最近 N 个月（合并实时 MQTT + 阿里云物模型历史） */
router.get('/', async (req, res, next) => {
  try {
    const deviceName = String(req.query.deviceName || config.aliyun.lockDeviceName).trim();
    const months = Math.min(12, Math.max(1, Number(req.query.months) || 3));
    const forceRefresh = req.query.refresh === '1' || req.query.refresh === 'true';

    let aliyunSync = { synced: false, reason: 'not_configured' };
    if (config.aliyun.openapiConfigured) {
      aliyunSync = await aliyunHistory.ensureSynced(deviceName, months, forceRefresh);
    }

    const records = recordStore.query(deviceName, months);
    const sinceMs = Date.now() - months * 30 * 24 * 60 * 60 * 1000;

    res.json({
      ok: true,
      deviceName,
      months,
      since: new Date(sinceMs).toISOString(),
      total: records.length,
      records,
      aliyunSync,
      aliyunHistoryEnabled: config.aliyun.openapiConfigured,
    });
  } catch (err) {
    next(err);
  }
});

/** GET /records/stats/today — 今日开锁次数 */
router.get('/stats/today', (req, res) => {
  const deviceName = String(req.query.deviceName || config.aliyun.lockDeviceName).trim();
  res.json({
    ok: true,
    deviceName,
    count: recordStore.countToday(deviceName),
  });
});

/** GET /records/:id — 单条详情 */
router.get('/:id', (req, res) => {
  const rec = recordStore.findById(req.params.id);
  if (!rec) {
    return res.status(404).json({ ok: false, error: 'NOT_FOUND', message: '记录不存在' });
  }
  res.json({ ok: true, record: rec });
});

module.exports = router;
