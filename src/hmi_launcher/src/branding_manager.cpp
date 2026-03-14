#include "hmi/branding_manager.hpp"

#include "common/config_loader.hpp"

#include <QFile>

namespace speeduino {

BrandingManager::BrandingManager(QObject* parent)
    : QObject(parent)
{
}

bool BrandingManager::showSplash() const
{
    return ConfigLoader::getBrandingConfig().show_splash;
}

int BrandingManager::splashDurationMs() const
{
    return static_cast<int>(ConfigLoader::getBrandingConfig().splash_duration_ms);
}

QString BrandingManager::splashLogo() const
{
    return QString::fromStdString(ConfigLoader::getBrandingConfig().splash_logo);
}

double BrandingManager::splashLogoScale() const
{
    return ConfigLoader::getBrandingConfig().splash_logo_scale;
}

QString BrandingManager::brandName() const
{
    return QString::fromStdString(ConfigLoader::getBrandingConfig().brand_name);
}

QString BrandingManager::brandColor() const
{
    return QString::fromStdString(ConfigLoader::getBrandingConfig().brand_color);
}

QString BrandingManager::manufacturer() const
{
    return QString::fromStdString(ConfigLoader::getBrandingConfig().manufacturer);
}

QString BrandingManager::splashLogoUrl() const
{
    QString logoPath = splashLogo();

    if (logoPath.isEmpty()) {
        return QString();
    }

    // Check if file exists
    if (!QFile::exists(logoPath)) {
        return QString();
    }

    // Return as file:// URL for QML Image source
    return QStringLiteral("file://") + logoPath;
}

}  // namespace speeduino
