import binascii
import os

os.chdir(r'C:\Users\valen\OneDrive\Documents\Projet_IOT')

print("="*70)
print("CONVERSION DU MODÈLE TFLITE EN CODE C")
print("="*70)

print(f"\nDossier de travail: {os.getcwd()}")

if not os.path.exists('epilepsy_model_quantized.tflite'):
    print("\n ERREUR: epilepsy_model_quantized.tflite non trouvé!")
    print(f"   Dossier actuel: {os.getcwd()}")
    print("\nFichiers présents dans ce dossier:")
    for f in os.listdir('.'):
        print(f"  - {f}")
    exit(1)

# Lire le fichier TFLite
with open('epilepsy_model_quantized.tflite', 'rb') as f:
    model = f.read()

print(f"\n✓ Modèle chargé: {len(model)} bytes ({len(model)/1024:.2f} KB)")

# Convertir en array C
with open('model_data.h', 'w') as f:
    f.write('// Modèle TensorFlow Lite - Détection Crises Épileptiques\n')
    f.write(f'// Taille: {len(model)} bytes ({len(model)/1024:.2f} KB)\n')
    f.write('// Accuracy: 99.46%\n')
    f.write('// Dataset: Epileptic Seizure Recognition\n\n')
    
    f.write('#ifndef MODEL_DATA_H\n')
    f.write('#define MODEL_DATA_H\n\n')
    
    f.write('const unsigned char model_data[] = {\n  ')
    hex_str = binascii.hexlify(model).decode('utf-8')
    
    bytes_written = 0
    for i in range(0, len(hex_str), 2):
        if i > 0 and bytes_written % 12 == 0:
            f.write('\n  ')
        f.write(f'0x{hex_str[i:i+2]}, ')
        bytes_written += 1
    
    f.write('\n};\n\n')
    f.write(f'const unsigned int model_data_len = {len(model)};\n\n')
    f.write('#endif // MODEL_DATA_H\n')

print(f"✓ Fichier model_data.h créé ({os.path.getsize('model_data.h')/1024:.1f} KB)\n")
print("CONVERSION RÉUSSIE!")
print("\nFichier généré: model_data.h")
