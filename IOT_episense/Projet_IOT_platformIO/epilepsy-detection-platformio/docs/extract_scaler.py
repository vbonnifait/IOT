import joblib
import numpy as np
import os

os.chdir(r'C:\Users\valen\OneDrive\Documents\Projet_IOT')

print("="*70)
print("EXTRACTION DES PARAMÈTRES DU SCALER")
print("="*70)

print(f"\nDossier de travail: {os.getcwd()}")

# Vérifier que le fichier existe
if not os.path.exists('scaler.pkl'):
    print("\n ERREUR: scaler.pkl non trouvé!")
    print(f"   Dossier actuel: {os.getcwd()}")
    print("\nFichiers .pkl présents:")
    for f in os.listdir('.'):
        if f.endswith('.pkl'):
            print(f"  - {f}")
    exit(1)

# Charger le scaler
scaler = joblib.load('scaler.pkl')

print(f"\n✓ Scaler chargé")
print(f"  Nombre de features: {len(scaler.mean_)}")

# Générer le code C
with open('scaler_params.h', 'w') as f:
    f.write('// Paramètres de normalisation (StandardScaler)\n')
    f.write('// Pour normaliser les features avant inférence\n\n')
    
    f.write('#ifndef SCALER_PARAMS_H\n')
    f.write('#define SCALER_PARAMS_H\n\n')
    
    f.write(f'const int NUM_FEATURES = {len(scaler.mean_)};\n\n')
    
    # Moyennes
    f.write('// Moyennes (mean_)\n')
    f.write('const float scaler_mean[NUM_FEATURES] = {\n')
    for i in range(0, len(scaler.mean_), 5):
        values = scaler.mean_[i:i+5]
        f.write('  ' + ', '.join([f'{v:.6f}f' for v in values]) + ',\n')
    f.write('};\n\n')
    
    # Écarts-types
    f.write('// Écarts-types (scale_)\n')
    f.write('const float scaler_scale[NUM_FEATURES] = {\n')
    for i in range(0, len(scaler.scale_), 5):
        values = scaler.scale_[i:i+5]
        f.write('  ' + ', '.join([f'{v:.6f}f' for v in values]) + ',\n')
    f.write('};\n\n')
    
    f.write('#endif // SCALER_PARAMS_H\n')

print(f"✓ Fichier scaler_params.h créé ({os.path.getsize('scaler_params.h')/1024:.1f} KB)\n")

# Afficher un aperçu
print("Aperçu des paramètres:")
print(f"  Premier mean: {scaler.mean_[0]:.6f}")
print(f"  Premier scale: {scaler.scale_[0]:.6f}")
print(f"  Dernier mean: {scaler.mean_[-1]:.6f}")
print(f"  Dernier scale: {scaler.scale_[-1]:.6f}")

print("\n" + "="*70)
print("FICHIERS GÉNÉRÉS POUR ARDUINO:")
print("="*70)
print("  ✓ model_data.h       (modèle TFLite en C)")
print("  ✓ scaler_params.h    (paramètres normalisation)")
print("\nVous pouvez maintenant les utiliser dans Arduino IDE!")
print("\nProchaine étape:")
print("  1. Ouvrir Arduino IDE")
print("  2. Créer un nouveau sketch")
print("  3. Copier le code epilepsy_detector_esp32.ino")
print("  4. Ajouter model_data.h et scaler_params.h au projet")
print("  5. Installer TensorFlowLite_ESP32")
print("  6. Compiler et uploader sur ESP32")
print("="*70)