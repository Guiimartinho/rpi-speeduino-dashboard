#ifndef HMI_BRANDING_MANAGER_HPP
#define HMI_BRANDING_MANAGER_HPP

#include <QObject>
#include <QString>

namespace speeduino {

/**
 * @brief QObject wrapper to expose branding configuration to QML
 *
 * This class reads branding settings from ConfigLoader and exposes them
 * as Q_PROPERTY for use in QML splash screens and UI customization.
 */
class BrandingManager : public QObject {
    Q_OBJECT

    // Splash screen properties
    Q_PROPERTY(bool showSplash READ showSplash CONSTANT)
    Q_PROPERTY(int splashDurationMs READ splashDurationMs CONSTANT)
    Q_PROPERTY(QString splashLogo READ splashLogo CONSTANT)
    Q_PROPERTY(double splashLogoScale READ splashLogoScale CONSTANT)

    // Brand properties
    Q_PROPERTY(QString brandName READ brandName CONSTANT)
    Q_PROPERTY(QString brandColor READ brandColor CONSTANT)
    Q_PROPERTY(QString manufacturer READ manufacturer CONSTANT)

public:
    explicit BrandingManager(QObject* parent = nullptr);
    ~BrandingManager() override = default;

    // Splash screen getters
    bool showSplash() const;
    int splashDurationMs() const;
    QString splashLogo() const;
    double splashLogoScale() const;

    // Brand getters
    QString brandName() const;
    QString brandColor() const;
    QString manufacturer() const;

    // Utility method to get logo URL for QML Image source
    Q_INVOKABLE QString splashLogoUrl() const;
};

}  // namespace speeduino

#endif  // HMI_BRANDING_MANAGER_HPP
