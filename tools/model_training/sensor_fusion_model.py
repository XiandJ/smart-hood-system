"""
传感器融合模型训练脚本
目标: 训练一个轻量级时序CNN模型，输入多传感器数据，输出风机档位和空气质量类别

输出:
  - sensor_fusion_model.h5       (Keras模型)
  - sensor_fusion_model.tflite   (TFLite模型)
  - sensor_fusion_model_quant.tflite (int8量化模型, 用于STM32Cube.AI部署)
"""

import numpy as np
import tensorflow as tf
from tensorflow import keras
from sklearn.model_selection import train_test_split
import json

# ============================================================
# 配置
# ============================================================
SEQ_LEN = 16          # 时间窗口 (16秒)
FEATURES = 8          # [T, RH, VOC, PM1.0, PM2.5, PM10, I_motor, dI/dt]
NUM_CLASSES = 6       # 风机档位 0-5
AQ_CLASSES = 4        # 空气质量类别: 优/良/中/差
QUANTIZE = True

BATCH_SIZE = 64
EPOCHS = 100
LEARNING_RATE = 0.001

# ============================================================
# 模型定义
# ============================================================

def build_sensor_fusion_model():
    """构建1D-CNN时序传感器融合模型"""

    inputs = keras.Input(shape=(SEQ_LEN, FEATURES), name='sensor_input')

    # --- 第一卷积块 ---
    x = keras.layers.Conv1D(filters=16, kernel_size=3, padding='same',
                             activation='relu', name='conv1')(inputs)
    x = keras.layers.BatchNormalization(name='bn1')(x)
    x = keras.layers.MaxPooling1D(pool_size=2, name='pool1')(x)  # (8, 16)

    # --- 第二卷积块 ---
    x = keras.layers.Conv1D(filters=32, kernel_size=3, padding='same',
                             activation='relu', name='conv2')(x)
    x = keras.layers.BatchNormalization(name='bn2')(x)
    x = keras.layers.MaxPooling1D(pool_size=2, name='pool2')(x)  # (4, 32)

    # --- 第三卷积块 ---
    x = keras.layers.Conv1D(filters=32, kernel_size=3, padding='same',
                             activation='relu', name='conv3')(x)
    x = keras.layers.BatchNormalization(name='bn3')(x)
    x = keras.layers.GlobalAveragePooling1D(name='gap')(x)  # (32,)

    # --- 全连接层 ---
    x = keras.layers.Dense(16, activation='relu', name='dense1')(x)
    x = keras.layers.Dropout(0.2, name='dropout')(x)

    # --- 主输出: 风机档位 ---
    fan_output = keras.layers.Dense(NUM_CLASSES, activation='softmax',
                                     name='fan_level')(x)

    # --- 辅助输出: 空气质量等级 ---
    aq_output = keras.layers.Dense(AQ_CLASSES, activation='softmax',
                                    name='air_quality')(x)

    model = keras.Model(inputs=inputs, outputs=[fan_output, aq_output],
                        name='sensor_fusion_model')

    model.compile(
        optimizer=keras.optimizers.Adam(learning_rate=LEARNING_RATE),
        loss={
            'fan_level': 'sparse_categorical_crossentropy',
            'air_quality': 'sparse_categorical_crossentropy',
        },
        loss_weights={
            'fan_level': 1.0,
            'air_quality': 0.5,
        },
        metrics=['accuracy']
    )

    return model


# ============================================================
# 数据生成 (模拟)
# ============================================================

def generate_synthetic_data(n_samples=10000):
    """
    生成合成训练数据
    实际部署前应替换为真实厨房环境采集的数据

    数据分布模拟:
      - 空闲: 低PM + 低VOC + 正常温湿度
      - 煮: 中PM + 中VOC + 高湿度 (蒸汽)
      - 炒: 高PM + 高VOC + 高温
      - 炸: 极高PM + 极高VOC + 高温 + 大电流
    """

    np.random.seed(42)

    scenarios = {
        'idle':    {'weight': 0.25, 'fan': 0, 'aq': 0,
                    'pm25': (0, 15),   'voc': (0, 30),    'temp': (20, 28),
                    'rh': (35, 60),    'i': (0.0, 0.1)},
        'boiling': {'weight': 0.25, 'fan': 2, 'aq': 1,
                    'pm25': (10, 40),  'voc': (20, 80),   'temp': (25, 35),
                    'rh': (65, 95),    'i': (0.1, 0.8)},
        'stir_fry': {'weight': 0.30, 'fan': 4, 'aq': 2,
                    'pm25': (50, 150), 'voc': (100, 300), 'temp': (30, 45),
                    'rh': (40, 70),    'i': (1.0, 3.0)},
        'deep_fry': {'weight': 0.20, 'fan': 5, 'aq': 3,
                    'pm25': (100, 300),'voc': (200, 500), 'temp': (35, 55),
                    'rh': (30, 55),    'i': (3.0, 8.0)},
    }

    X = np.zeros((n_samples, SEQ_LEN, FEATURES), dtype=np.float32)
    y_fan = np.zeros((n_samples,), dtype=np.int32)
    y_aq = np.zeros((n_samples,), dtype=np.int32)

    scenario_names = list(scenarios.keys())
    weights = [scenarios[s]['weight'] for s in scenario_names]
    weights = np.array(weights) / np.sum(weights)

    for i in range(n_samples):
        scenario = scenarios[np.random.choice(scenario_names, p=weights)]

        pm25  = np.clip(np.random.normal(
            np.mean(scenario['pm25']),  np.std(scenario['pm25'])/2,
            SEQ_LEN), 0, 500)
        voc   = np.clip(np.random.normal(
            np.mean(scenario['voc']),   np.std(scenario['voc'])/2,
            SEQ_LEN), 0, 500)
        temp  = np.clip(np.random.normal(
            np.mean(scenario['temp']),  3, SEQ_LEN), 0, 80)
        rh    = np.clip(np.random.normal(
            np.mean(scenario['rh']),    8, SEQ_LEN), 0, 100)
        motor_i = np.clip(np.random.normal(
            np.mean(scenario['i']), np.std(scenario['i'])/2 + 0.1,
            SEQ_LEN), 0, 10)
        di_dt = np.diff(motor_i, prepend=motor_i[0])

        X[i, :, 0] = temp
        X[i, :, 1] = rh
        X[i, :, 2] = voc
        X[i, :, 3] = pm25 * 0.3
        X[i, :, 4] = pm25
        X[i, :, 5] = pm25 * 1.2
        X[i, :, 6] = motor_i
        X[i, :, 7] = di_dt

        y_fan[i] = scenario['fan']
        y_aq[i] = scenario['aq']

    return X, y_fan, y_aq


# ============================================================
# 训练主流程
# ============================================================

def main():
    print("=" * 60)
    print("Smart Hood - Sensor Fusion Model Training")
    print("=" * 60)

    # 1. 生成/加载数据
    print("\n[1/5] Generating synthetic training data...")
    X, y_fan, y_aq = generate_synthetic_data(10000)
    print(f"  Data shape: X={X.shape}, fan_labels={y_fan.shape}, aq_labels={y_aq.shape}")

    # 2. 数据集分割
    print("\n[2/5] Splitting dataset...")
    X_train, X_test, y_fan_train, y_fan_test, y_aq_train, y_aq_test = \
        train_test_split(X, y_fan, y_aq, test_size=0.2, random_state=42,
                         stratify=np.stack([y_fan, y_aq], axis=1))
    print(f"  Train: {len(X_train)}, Test: {len(X_test)}")

    # 3. 构建模型
    print("\n[3/5] Building model...")
    model = build_sensor_fusion_model()
    model.summary()

    # 4. 训练
    print("\n[4/5] Training...")
    callbacks = [
        keras.callbacks.EarlyStopping(monitor='val_loss', patience=15,
                                       restore_best_weights=True),
        keras.callbacks.ReduceLROnPlateau(monitor='val_loss', factor=0.5,
                                           patience=5, min_lr=1e-6),
        keras.callbacks.ModelCheckpoint('best_model.h5', monitor='val_loss',
                                         save_best_only=True),
    ]

    history = model.fit(
        X_train,
        {'fan_level': y_fan_train, 'air_quality': y_aq_train},
        validation_data=(X_test,
                         {'fan_level': y_fan_test, 'air_quality': y_aq_test}),
        batch_size=BATCH_SIZE,
        epochs=EPOCHS,
        callbacks=callbacks,
        verbose=2
    )

    # 5. 评估
    print("\n[5/5] Evaluating...")
    results = model.evaluate(X_test,
                             {'fan_level': y_fan_test, 'air_quality': y_aq_test},
                             verbose=0)
    print(f"  Test Loss: {results[0]:.4f}")
    print(f"  Fan Level Accuracy: {results[3]*100:.1f}%")
    print(f"  Air Quality Accuracy: {results[4]*100:.1f}%")

    # 保存Keras模型
    model.save('sensor_fusion_model.h5')
    print("  Saved: sensor_fusion_model.h5")

    # ============================================================
    # TFLite 转换
    # ============================================================
    print("\n--- TFLite Conversion ---")

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    tflite_model = converter.convert()
    with open('sensor_fusion_model.tflite', 'wb') as f:
        f.write(tflite_model)

    interpreter = tf.lite.Interpreter(model_content=tflite_model)
    interpreter.allocate_tensors()
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    print(f"  Float32 model size: {len(tflite_model)} bytes")
    print(f"  Input:  {input_details[0]['shape']} {input_details[0]['dtype']}")
    print(f"  Output: {output_details[0]['shape']} {output_details[0]['dtype']}")
    print(f"  Output: {output_details[1]['shape']} {output_details[1]['dtype']}")

    # ============================================================
    # Int8 量化
    # ============================================================
    if QUANTIZE:
        print("\n--- Int8 Quantization ---")

        def representative_dataset():
            for i in range(200):
                yield [X_train[i:i+1].astype(np.float32)]

        converter = tf.lite.TFLiteConverter.from_keras_model(model)
        converter.optimizations = [tf.lite.Optimize.DEFAULT]
        converter.representative_dataset = representative_dataset
        converter.target_spec.supported_ops = [
            tf.lite.OpsSet.TFLITE_BUILTINS_INT8
        ]
        converter.inference_input_type = tf.int8
        converter.inference_output_type = tf.int8

        quant_model = converter.convert()
        with open('sensor_fusion_model_quant.tflite', 'wb') as f:
            f.write(quant_model)
        print(f"  Int8 model size: {len(quant_model)} bytes")
        print(f"  Compression ratio: {len(tflite_model)/len(quant_model):.1f}x")

    # ============================================================
    # 保存归一化参数
    # ============================================================
    norm_params = {
        'mean': [float(np.mean(X[:, :, i])) for i in range(FEATURES)],
        'std':  [float(np.std(X[:, :, i])) for i in range(FEATURES)],
        'features': ['temperature', 'humidity', 'voc', 'pm1.0', 'pm2.5',
                     'pm10', 'motor_current', 'di_dt'],
        'seq_len': SEQ_LEN,
        'num_classes': NUM_CLASSES,
        'aq_classes': AQ_CLASSES,
    }

    with open('norm_params.json', 'w') as f:
        json.dump(norm_params, f, indent=2)
    print("\n  Saved: norm_params.json")

    print("\n=== Training Complete ===")
    print("Deploy int8 model via STM32Cube.AI: sensor_fusion_model_quant.tflite")


if __name__ == "__main__":
    main()
